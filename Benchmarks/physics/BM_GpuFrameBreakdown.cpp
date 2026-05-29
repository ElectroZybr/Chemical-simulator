// Phase 0 (measure-first): декомпозиция стоимости ОДНОГО per-frame кадра в
// GPU-режиме, чтобы решить, оправдана ли будущая "zero-copy render"
// оптимизация. Вопрос: per-frame download (downloadToCpu pos+vel, блокирующий
// poll) + render CPU-upload (pack атрибутов + writeBuffer) — это значимая доля
// GPU-кадра, или дёшево на фоне physics-шага и NL rebuild?
//
// НИЧЕГО не меняет в рантайме: бенч АДДИТИВЕН. Компоненты 1-3,5 меряются на
// СОБСТВЕННОМ экземпляре GpuResidentPhysics (public API uploadFromCpu/step/
// downloadToCpu/rebuildNeighborListOnGpu) — тот же приём, что BM_GpuFullStep_
// Resident (bench-owned инстанс, shipped-путь не трогаем). Каденция rebuild и
// component 5 берутся из РЕАЛЬНОГО пути Simulation в GPU-режиме. Component 4
// (render upload) реплицирует pack+writeBuffer из RendererWGPU::drawAtomsImpl
// (RendererWGPU.cpp:464-489) на bench-owned storage-буфера — drawAtomsImpl
// приватен, а добавлять bench-хук в shipped рендер-класс мы не хотим.
//
// Методология (как RESULTS.md): repetitions=3, min_time=0.3s, median, cv.
// Представительные N = 15625 и 103823 (размеры doc'а).
//
// PITFALL (из прошлых замеров): блокирующий poll доминирует wall-clock. Поэтому
// дешёвые GPU-операции (step) батчатся: K шагов подряд (каждый — свой queue.submit,
// БЕЗ poll внутри) + один poll(true) после всех K — фиксированная стоимость drain'а
// амортизируется по батчу, per-op = wall/K. Операции, которые САМИ делают
// блокирующий poll внутри (downloadToCpu — map+poll; rebuildNeighborListOnGpu —
// скалярный readback в builder'е), меряются поштучно: их per-call стоимость и ЕСТЬ
// та цена, которую платит кадр (и которую убирает zero-copy для download'а).

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <benchmark/benchmark.h>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Benchmarks/fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/Simulation.h"
#include "Engine/World.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"
#include "Rendering/WGPUContext.h"

namespace {

// --- Общая сцена для компонентов 1-3 (резидентный bench-owned инстанс) ---
// Кубическая решётка, LJ-only, Full NL, КОГЕРЕНТНЫЙ дрейф vx=5 (как
// BM_GpuFullStep_WithRebuild): надёжно триггерит disp-каденцию для component 2,
// но без относительного движения (LJ не взрывается на длинном прогоне).
struct ResidentScene {
    Simulation sim;
    GpuResidentPhysics gpu;       // bench-owned: мутирующие методы вызываем напрямую
    AtomStorage downloadTarget;   // приёмник для downloadToCpu (component 3)
    float worldSize = 0.0f;
    uint32_t totalCount = 0;

    void build(int atomCount) {
        benchmarkDevice();
        const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount))) + 1;
        const float spacing = 3.0f;
        worldSize = side * spacing + 20.0f;

        sim.createWorld(Vec3f{worldSize, worldSize, worldSize});
        sim.setSizeBox(sim.world().getWorldSize(), 6);
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setDt(0.01f);
        sim.neighborList().setMode(NeighborListMode::Full);
        int placed = 0;
        for (int z = 0; z < side && placed < atomCount; ++z) {
            for (int y = 0; y < side && placed < atomCount; ++y) {
                for (int x = 0; x < side && placed < atomCount; ++x) {
                    sim.appendAtomFast(Vec3f{10.0f + x * spacing, 10.0f + y * spacing, 10.0f + z * spacing},
                                       Vec3f{5.0f, 0.0f, 0.0f}, AtomData::Type::H, false); // когерентный дрейф
                    ++placed;
                }
            }
        }
        sim.finalizeAtomBatch();
        sim.neighborList().build(sim.atoms(), sim.world());

        // Заливаем сцену в bench-owned резидентный инстанс (public API).
        const Vec3f size = sim.world().getWorldSize();
        gpu.uploadFromCpu(sim.atoms(), sim.neighborList(), LJForceField{}, static_cast<float>(size.x),
                          static_cast<float>(size.y), static_cast<float>(size.z), 0.0f, 0.0f, 0.0f); // gravity=0 (perf-сцена)
        totalCount = gpu.totalCount();

        // Приёмник download'а: тот же размер, что резидентная сцена (downloadToCpu
        // пишет min(totalCount, target.size()) атомов). AtomStorage некопируем —
        // наполняем заглушками (позиции перезапишет сам download).
        downloadTarget.reserve(static_cast<size_t>(totalCount));
        for (uint32_t i = 0; i < totalCount; ++i) {
            downloadTarget.addAtom(Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::H, false);
        }
    }

    // Параметры сетки для rebuildNeighborListOnGpu (как Simulation::updateStateGpu).
    const SpatialGrid& grid() const { return sim.world().getGrid(); }
    float listRadiusSqr() const {
        const float lr = sim.world().getNeighborList().listRadius();
        return lr * lr;
    }
};

// =====================================================================
// Component 1 — physics step (predict+confine+zero+LJ+correct, резидентный).
// Батчим K шагов (каждый — свой queue.submit, внутри step() нет poll'а), один
// poll(true) после всех K. per-step = wall/K. Дублирует методологию BM_GpuFullStep_Resident,
// но через РЕАЛЬНЫЙ GpuResidentPhysics::step (а не локальную копию пайплайна) —
// это и есть shipped step(). Сравнение с Resident-числами = sanity-check.
// =====================================================================
constexpr int kStepBatch = 50;

void runStep(benchmark::State& state) {
    const int atomCount = static_cast<int>(state.range(0));
    ResidentScene s;
    s.build(atomCount);
    // warmup
    for (int i = 0; i < 4; ++i) {
        s.gpu.step(0.01f, 0.9f);
    }
    WGPUContext::instance().device()->poll(true, nullptr);

    for (auto _ : state) {
        for (int i = 0; i < kStepBatch; ++i) {
            s.gpu.step(0.01f, 0.9f); // только submit, без poll
        }
        WGPUContext::instance().device()->poll(true, nullptr); // один drain на батч
    }
    // per-step = report Time / kStepBatch (Time = wall за kStepBatch шагов + 1 poll).
    state.counters["steps_per_iter"] = kStepBatch;
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kStepBatch) * atomCount);
}

// =====================================================================
// Component 2 — GPU NL rebuild (rebuildNeighborListOnGpu). Внутри: cell-list +
// scan + Full NL build + GPU->GPU copy в резидентные буфера. Builder делает
// СКАЛЯРНЫЙ блокирующий readback (total соседей), поэтому каждый вызов несёт
// внутренний sync — батчить нельзя, меряем поштучно (1 итерация = 1 rebuild +
// poll(true) для полного wall). Это per-rebuild стоимость; амортизация по
// каденции считается отдельным бенчем (real Simulation путь).
//
// ВАЖНО: между rebuild'ами НЕ шагаем (иначе LJ-дрейф изменит позиции и rebuild
// будет на сдвинутой сцене — это ок для тайминга, но мы хотим чистую цену
// перестройки на стабильной решётке). Каждый rebuild перестраивает из тех же
// резидентных позиций — стоимость постройки от шага не зависит.
// =====================================================================
void runNlRebuild(benchmark::State& state) {
    const int atomCount = static_cast<int>(state.range(0));
    ResidentScene s;
    s.build(atomCount);
    const SpatialGrid& g = s.grid();
    const float lrSqr = s.listRadiusSqr();
    // warmup (lazy-инициализация builder'а + прогрев пайплайнов)
    s.gpu.rebuildNeighborListOnGpu(g.size.x, g.size.y, g.size.z, g.cellSize, static_cast<uint32_t>(g.countCells), lrSqr);
    WGPUContext::instance().device()->poll(true, nullptr);

    for (auto _ : state) {
        s.gpu.rebuildNeighborListOnGpu(g.size.x, g.size.y, g.size.z, g.cellSize, static_cast<uint32_t>(g.countCells), lrSqr);
        WGPUContext::instance().device()->poll(true, nullptr); // полный wall перестройки
    }
    state.counters["rebuilds_total"] = static_cast<double>(s.gpu.nlRebuildCount());
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(atomCount));
}

// =====================================================================
// Component 3 — per-frame download (downloadToCpu pos+vel). ЭТО zero-copy цель.
// downloadToCpu сам делает map(pos)+map(vel)+блокирующий poll до готовности обоих
// — один блокирующий sync на вызов. Меряем поштучно: 1 итерация = 1 download
// (точно как syncFromGpuIfNeeded зовёт его раз в кадр). Per-call = цена, которую
// убирает zero-copy.
//
// withVelocities=true — РЕАЛЬНЫЙ per-frame путь (Simulation::syncFromGpuIfNeeded
// всегда качает pos+vel: vel нужен для speed-color рендера). Отдельно меряем
// pos-only, чтобы показать вклад скоростей.
// =====================================================================
void runDownload(benchmark::State& state) {
    const int atomCount = static_cast<int>(state.range(0));
    const bool withVel = state.range(1) != 0;
    ResidentScene s;
    s.build(atomCount);
    // warmup + один шаг, чтобы был реальный контент для скачивания
    s.gpu.step(0.01f, 0.9f);
    WGPUContext::instance().device()->poll(true, nullptr);
    s.gpu.downloadToCpu(s.downloadTarget, withVel);

    for (auto _ : state) {
        s.gpu.downloadToCpu(s.downloadTarget, withVel); // блокирующий map+poll внутри
    }
    state.counters["with_velocities"] = withVel ? 1.0 : 0.0;
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(atomCount));
}

// =====================================================================
// Component 4 — renderer CPU-upload. Реплицирует RendererWGPU::drawAtomsImpl
// pack+writeBuffer (RendererWGPU.cpp:464-489) на bench-owned storage-буфера.
// Меряем: CPU pack 5 атрибутов (pos/vel/radius/type/sel) + 5 writeBuffer +
// poll(true) (захватить реальный трансфер). drawAtomsImpl приватен; bench-хук в
// shipped рендер-класс не добавляем — реплика 1:1 покрывает измеряемую работу.
//
// zero-copy убирает ТОЛЬКО pos+vel pack+upload (рендер читал бы их из
// резидентных GPU-буферов); type/radius/sel остаются renderer-owned (RESULTS.md).
// Поэтому отдельно меряем "pos+vel only" (zero-copy-relevant) и "all 5" (полный
// per-draw upload). pad-байт и AtomVec4 layout — те же, что в RendererWGPU.h.
// =====================================================================
struct AtomVec4 {
    float x, y, z, pad = 0.0f;
};

// Args: {N, posVelOnly, stage}.
//   posVelOnly: 0 = все 5 атрибутов (полный per-draw upload), 1 = только pos+vel
//               (zero-copy-relevant подмножество).
//   stage:      0 = pack только (чистая CPU-стоимость упаковки массивов, без GPU);
//               1 = pack + writeBuffer issue, БЕЗ poll (маргинальная стоимость в
//                   реальном кадре: writeBuffer стейджит копию синхронно на CPU,
//                   сам трансфер едет на общем frame-submit'е — рендер НЕ делает
//                   отдельный drain под upload).
// Меряем CPU-доли (stage 0/1) — именно их zero-copy убирает для pos+vel. Transfer-
// inclusive вариант (пустой submit + poll(true)) НЕ меряем: poll поверх пустого
// сабмита ловит jitter планировщика (cv >150%), не реальный трансфер. Верхнюю
// границу полного кадра берём из реального DrawShot3D (полный кадр).
void runRenderUpload(benchmark::State& state) {
    const int atomCount = static_cast<int>(state.range(0));
    const bool posVelOnly = state.range(1) != 0;
    const int stage = static_cast<int>(state.range(2));
    benchmarkDevice();

    // Сцена-источник (CPU AtomStorage), как RendererFixture::makeGridAtoms.
    AtomStorage atoms;
    atoms.reserve(atomCount);
    const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount))) + 1;
    for (int i = 0; i < atomCount; ++i) {
        atoms.addAtom(Vec3f((i % side) * 3.0f, ((i / side) % side) * 3.0f, (i / static_cast<float>(side * side)) * 3.0f),
                      Vec3f::Random() * 0.5f, AtomData::Type::H);
    }
    const size_t count = atoms.size();

    // Bench-owned storage-буфера (как RendererWGPU::ensureStorageBuffers usage:
    // Storage|CopyDst). writeBuffer пишет туда — захватываем реальную стоимость.
    const wgpu::BufferUsage usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    wgpu::raii::Buffer sbPos = WGPUContext::instance().createBuffer(count * sizeof(AtomVec4), usage, "BD_Pos");
    wgpu::raii::Buffer sbVel = WGPUContext::instance().createBuffer(count * sizeof(AtomVec4), usage, "BD_Vel");
    wgpu::raii::Buffer sbType = WGPUContext::instance().createBuffer(count * sizeof(float), usage, "BD_Type");
    wgpu::raii::Buffer sbRadius = WGPUContext::instance().createBuffer(count * sizeof(float), usage, "BD_Radius");
    wgpu::raii::Buffer sbSel = WGPUContext::instance().createBuffer(count * sizeof(float), usage, "BD_Sel");

    // Scratch-вектора (как члены RendererWGPU: переиспользуются между кадрами).
    std::vector<AtomVec4> posData(count), velData(count);
    std::vector<float> radii(count), typeData(count), selectedData(count, 0.0f);
    auto q = WGPUContext::instance().queue();

    const bool doUpload = stage >= 1; // stage 0 = pack-only (без writeBuffer)
    auto packAndUpload = [&]() {
        // pack pos+vel (zero-copy-relevant) — всегда.
        for (size_t i = 0; i < count; ++i) {
            posData[i] = {atoms.xData()[i], atoms.yData()[i], atoms.zData()[i]};
            velData[i] = {atoms.vxData()[i], atoms.vyData()[i], atoms.vzData()[i]};
        }
        if (doUpload) {
            q->writeBuffer(*sbPos, 0, posData.data(), count * sizeof(AtomVec4));
            q->writeBuffer(*sbVel, 0, velData.data(), count * sizeof(AtomVec4));
        }
        benchmark::DoNotOptimize(posData.data());
        benchmark::DoNotOptimize(velData.data());
        if (!posVelOnly) {
            // pack type/radius/sel (renderer-owned, zero-copy НЕ убирает).
            for (size_t i = 0; i < count; ++i) {
                const auto& props = AtomData::getProps(atoms.type(i));
                radii[i] = props.radius;
                typeData[i] = static_cast<float>(atoms.type(i));
            }
            selectedData.assign(count, 0.0f);
            if (doUpload) {
                q->writeBuffer(*sbRadius, 0, radii.data(), count * sizeof(float));
                q->writeBuffer(*sbType, 0, typeData.data(), count * sizeof(float));
                q->writeBuffer(*sbSel, 0, selectedData.data(), count * sizeof(float));
            }
            benchmark::DoNotOptimize(radii.data());
            benchmark::DoNotOptimize(typeData.data());
        }
    };

    packAndUpload(); // warmup
    if (stage >= 1) {
        WGPUContext::instance().device()->poll(true, nullptr);
    }

    for (auto _ : state) {
        packAndUpload();
    }
    state.counters["pos_vel_only"] = posVelOnly ? 1.0 : 0.0;
    state.counters["stage"] = stage;
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(atomCount));
}

// =====================================================================
// Component 5 — refreshDiagnosticsGrid (CPU SpatialGrid re-bin). УСЛОВНО: только
// если drawGrid вкл. Меряем РЕАЛЬНЫЙ Simulation::refreshDiagnosticsGrid (CPU-
// only — перебиннивает грид из УЖЕ синканных позиций; VRAM не трогает).
// Precondition реального пути: syncFromGpuIfNeeded сделан в начале кадра. Мы его
// делаем в setup, потом крутим refresh поштучно. Это чистая CPU-стоимость
// перебиннивания (rebuild грида), без download'а.
// =====================================================================
void runRefreshGrid(benchmark::State& state) {
    const int atomCount = static_cast<int>(state.range(0));
    const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount))) + 1;
    const float spacing = 3.0f;
    const float worldSize = side * spacing + 20.0f;

    Simulation sim;
    sim.createWorld(Vec3f{worldSize, worldSize, worldSize});
    sim.setSizeBox(sim.world().getWorldSize(), 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setDt(0.01f);
    int placed = 0;
    for (int z = 0; z < side && placed < atomCount; ++z) {
        for (int y = 0; y < side && placed < atomCount; ++y) {
            for (int x = 0; x < side && placed < atomCount; ++x) {
                sim.appendAtomFast(Vec3f{10.0f + x * spacing, 10.0f + y * spacing, 10.0f + z * spacing},
                                   Vec3f{5.0f, 0.0f, 0.0f}, AtomData::Type::H, false);
                ++placed;
            }
        }
    }
    sim.finalizeAtomBatch();
    sim.setGpuMode(true);
    for (int i = 0; i < 4; ++i) {
        sim.update(); // продвинуть GPU, чтобы позиции уехали
    }
    sim.syncFromGpuIfNeeded(); // precondition реального refresh: свежие CPU-позиции

    sim.refreshDiagnosticsGrid(); // warmup

    for (auto _ : state) {
        sim.refreshDiagnosticsGrid(); // чистый CPU re-bin (rebuild грида)
    }
    sim.setGpuMode(false);
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(atomCount));
}

} // namespace

// @bench_meta {"id":"GpuFrameBreakdown/Step","ru":"GPU кадр: physics step (резидентный)","group":"Симуляция/GPU"}
void BM_GpuFrameBreakdown_Step(benchmark::State& state) { runStep(state); }
BENCHMARK(BM_GpuFrameBreakdown_Step)->Arg(15625)->Arg(103823)->Unit(benchmark::kMicrosecond);

// @bench_meta {"id":"GpuFrameBreakdown/NlRebuild","ru":"GPU кадр: NL rebuild (per-call)","group":"Симуляция/GPU"}
void BM_GpuFrameBreakdown_NlRebuild(benchmark::State& state) { runNlRebuild(state); }
BENCHMARK(BM_GpuFrameBreakdown_NlRebuild)->Arg(15625)->Arg(103823)->Unit(benchmark::kMicrosecond);

// @bench_meta {"id":"GpuFrameBreakdown/Download","ru":"GPU кадр: download pos+vel (zero-copy цель)","group":"Симуляция/GPU"}
void BM_GpuFrameBreakdown_Download(benchmark::State& state) { runDownload(state); }
// Args: {N, withVelocities}. withVel=1 — реальный per-frame путь; =0 — pos-only вклад.
BENCHMARK(BM_GpuFrameBreakdown_Download)
    ->Args({15625, 1})
    ->Args({15625, 0})
    ->Args({103823, 1})
    ->Args({103823, 0})
    ->Unit(benchmark::kMicrosecond);

// @bench_meta {"id":"GpuFrameBreakdown/RenderUpload","ru":"GPU кадр: render pack+upload атрибутов","group":"Симуляция/GPU"}
void BM_GpuFrameBreakdown_RenderUpload(benchmark::State& state) { runRenderUpload(state); }
// Args: {N, posVelOnly, stage}. posVelOnly: 1 = zero-copy-relevant (pos+vel),
// 0 = все 5. stage: 0 = pack-only (CPU), 1 = pack+writeBuffer (без drain, реальный
// кадр). Transfer-inclusive stage убран — poll поверх пустого сабмита ловил jitter
// планировщика (cv >150%), не реальный трансфер.
BENCHMARK(BM_GpuFrameBreakdown_RenderUpload)
    ->Args({15625, 0, 0})
    ->Args({15625, 0, 1})
    ->Args({15625, 1, 0})
    ->Args({15625, 1, 1})
    ->Args({103823, 0, 0})
    ->Args({103823, 0, 1})
    ->Args({103823, 1, 0})
    ->Args({103823, 1, 1})
    ->Unit(benchmark::kMicrosecond);

// @bench_meta {"id":"GpuFrameBreakdown/RefreshGrid","ru":"GPU кадр: refreshDiagnosticsGrid (условно, drawGrid)","group":"Симуляция/GPU"}
void BM_GpuFrameBreakdown_RefreshGrid(benchmark::State& state) { runRefreshGrid(state); }
BENCHMARK(BM_GpuFrameBreakdown_RefreshGrid)->Arg(15625)->Arg(103823)->Unit(benchmark::kMicrosecond);
