// Parity-гейт переноса soft-wall + gravity на GPU (фаза 2.1). GPU-траектория с
// активными стенами и НЕНУЛЕВОЙ гравитацией должна совпасть с CPU velocity Verlet
// (WallForceField + LJForceField) в пределах fp-tolerance. Без этого «GPU считает
// стены/гравитацию» — недоказанное утверждение.
//
// Отличия от BM_GpuCorrectness (тот тестирует LJ-only, gravity=0, атомы глубоко
// внутри box — wall-kernel там прибавляет 0):
//   1) GPU-сторона гоняется через РЕАЛЬНЫЙ путь Simulation::setGpuMode(true) +
//      updateAll() (а не голый GpuResidentPhysics), чтобы gravity пришла из World
//      через uploadSceneToGpu/uploadFromCpu — тот же путь, что у приложения. CPU-
//      сторона — обычный updateAll() (ForceField::compute → wall → LJ).
//   2) Сцена активирует стену: часть атомов в зоне border=2 от грани (малый box).
//   3) Ненулевая gravity + СМЕШАННЫЕ массы (H ~1.008 и Ar ~39.948, ~40×): gravity
//      добавляется как постоянная СИЛА (не ускорение, WallForceField.cpp:47-51),
//      поэтому при разных массах ускорения разные — паритет ловит, что GPU копирует
//      именно силу, а не «чинит» на ускорение.
//
// Два кейса:
//   A) Static gravity: gravity задана ДО setGpuMode, прогон без правок.
//   B) Runtime gravity change: setGpuMode → шаги → setGravity(новое) → шаги →
//      сверка. Проверяет, что рантайм-смена gravity (через cpuSceneVersion-бамп в
//      Simulation::setGravity → re-upload) реально доходит до GPU, а не остаётся
//      устаревшей. Устаревшая gravity дала бы расхождение порядка 0.5*ΔG*t²
//      (вся компонента gravity неверна) — на порядки выше fp-transient'а.
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре.
// Бросает при расхождении выше tolerance — прогон падает (как BM_GpuCorrectness).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
using namespace Lattice;

namespace {

constexpr float kDt = 0.01f;
constexpr float kAccelDamping = 0.9f;
constexpr int kCellSize = 6;

// Общая конфигурация sim (CPU и GPU одинаковы, кроме режима). Gravity ставит
// вызывающий ДО setGpuMode (для GPU) — чтобы первичный upload её нёс.
void configureSim(Simulation& sim, float worldSize) {
    sim.createWorld(glm::vec3{worldSize, worldSize, worldSize});
    sim.setSizeBox(sim.world().getWorldSize(), kCellSize);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setDt(kDt);
    sim.setAccelDamping(kAccelDamping);
}

// Сцена «у стены»: атомы у нижней грани Y (coord ~1.4, penLow=0.6 → wall-сила
// 500*0.6^6 ≈ 23, в +, ограничена). Разнесены в плоскости стены (X,Z) на >=
// cutoff(5), чтобы LJ-соседей не было (NL пуст) — изолируем wall+gravity, исключаем
// дрейф NL-rebuild. Чередуем H/Ar (разные массы) — gravity-как-сила даёт разные
// ускорения. confine не срабатывает: атомы внутри [0,max], wall толкает внутрь.
// worldSize должен вместить сетку (макс. координата 3+2*6=15 <= max=worldSize-1).
void fillWallScene(Simulation& sim, float worldSize) {
    (void)worldSize; // сетка 3+2*6=15 помещается при worldSize>=16 (см. вызов)
    const float y0 = 1.4f; // в зоне border=2 от нижней грани Y (penLow = 0.6)
    int idx = 0;
    for (int gx = 0; gx < 3; ++gx) {
        for (int gz = 0; gz < 3; ++gz) {
            const AtomData::Type t = (idx % 2 == 0) ? AtomData::Type::H : AtomData::Type::Ar;
            // Шаг 6 в плоскости (X,Z) >= cutoff+skin: пары вне списка соседей.
            sim.appendAtomFast(glm::vec3{3.0f + gx * 6.0f, y0, 3.0f + gz * 6.0f}, glm::vec3{0.0f, 0.0f, 0.0f}, t, false);
            ++idx;
        }
    }
    sim.finalizeAtomBatch();
}

// Сцена «глубоко внутри»: атомы вдали от граней (wall-сила = 0), только gravity
// активна. Для runtime-кейса: re-upload на смене gravity обнуляет force-history
// (forces_[0/1]=0, как любая правка сцены) — с малой силой (только gravity) этот
// одношаговый transient мал. Изолирует доставку gravity.
void fillInteriorScene(Simulation& sim, float worldSize) {
    const float c = worldSize * 0.5f; // центр box, далеко от всех границ
    int idx = 0;
    for (int gx = 0; gx < 3; ++gx) {
        for (int gz = 0; gz < 3; ++gz) {
            const AtomData::Type t = (idx % 2 == 0) ? AtomData::Type::H : AtomData::Type::Ar;
            sim.appendAtomFast(glm::vec3{c - 6.0f + gx * 6.0f, c, c - 6.0f + gz * 6.0f}, glm::vec3{0.0f, 0.0f, 0.0f}, t, false);
            ++idx;
        }
    }
    sim.finalizeAtomBatch();
}

// max|pos_a - pos_b| по всем атомам (порядок детерминирован — сцены строятся
// одинаково на обеих сторонах).
double maxAbsPositionDiff(const AtomStorage& a, const AtomStorage& b) {
    double maxAbs = 0.0;
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        for (int c = 0; c < 3; ++c) {
            const float av = (c == 0) ? a.posX(i) : (c == 1) ? a.posY(i) : a.posZ(i);
            const float bv = (c == 0) ? b.posX(i) : (c == 1) ? b.posY(i) : b.posZ(i);
            maxAbs = std::max(maxAbs, std::abs(static_cast<double>(av) - static_cast<double>(bv)));
        }
    }
    return maxAbs;
}

// === Кейс A: static gravity, атомы у стены. ===
// Чистый паритет wall+gravity без правок в hot loop → tight tolerance.
double runStaticCase() {
    const float worldSize = 24.0f; // max=23; вмещает сетку (X,Z в {3,9,15}); нижняя стена Y [0,2]
    const glm::vec3 gravity{0.0f, -5.0f, 0.0f};
    constexpr int kSteps = 50;

    Simulation cpu;
    configureSim(cpu, worldSize);
    cpu.setGravity(gravity);
    fillWallScene(cpu, worldSize);

    Simulation gpu;
    configureSim(gpu, worldSize);
    gpu.setGravity(gravity); // ДО setGpuMode → первичный upload несёт gravity
    fillWallScene(gpu, worldSize);
    gpu.setGpuMode(true);

    for (int s = 0; s < kSteps; ++s) {
        cpu.updateAll();
        gpu.updateAll();
    }
    gpu.syncFromGpuIfNeeded(); // стянуть резидентные позиции в CPU-копию для сверки

    const double maxAbs = maxAbsPositionDiff(cpu.atoms(), gpu.atoms());
    std::printf("[ WALLGRAV ] static  : steps=%d gravityY=%.1f types=H/Ar max|dCPU-GPU| = %.3e abs\n", kSteps,
                static_cast<double>(gravity.y), maxAbs);
    return maxAbs;
}

// === Кейс B: runtime gravity change. ===
// setGpuMode → шаги → setGravity(новое) → шаги → сверка с CPU, у которого gravity
// сменили в тот же момент. Проверяет, что GPU реально перезалил новую gravity.
// Возвращает пару {maxAbs паритета, maxAbs против «устаревшей gravity»} — второе
// показывает, насколько грубо разошлось бы без фикса доставки (sanity-разрыв).
struct RuntimeResult {
    double parity;       // GPU(новая g) vs CPU(новая g) — должно быть мало
    double staleGapRef;  // CPU(новая g) vs CPU(старая g) — масштаб «если бы g не сменилась»
};
RuntimeResult runRuntimeCase() {
    const float worldSize = 20.0f;
    const glm::vec3 g0{0.0f, -3.0f, 0.0f};
    const glm::vec3 g1{2.0f, 4.0f, 0.0f}; // смена и направления, и величины
    constexpr int kStepsBefore = 20;
    constexpr int kStepsAfter = 25;

    // CPU reference: меняет gravity в тот же момент, что GPU.
    Simulation cpu;
    configureSim(cpu, worldSize);
    cpu.setGravity(g0);
    fillInteriorScene(cpu, worldSize);

    // GPU через реальный путь.
    Simulation gpu;
    configureSim(gpu, worldSize);
    gpu.setGravity(g0);
    fillInteriorScene(gpu, worldSize);
    gpu.setGpuMode(true);

    // «Стейл»-эталон: CPU, который НЕ меняет gravity (для масштаба разрыва).
    Simulation cpuStale;
    configureSim(cpuStale, worldSize);
    cpuStale.setGravity(g0);
    fillInteriorScene(cpuStale, worldSize);

    for (int s = 0; s < kStepsBefore; ++s) {
        cpu.updateAll();
        gpu.updateAll();
        cpuStale.updateAll();
    }

    // Рантайм-смена gravity на CPU и GPU (cpuStale оставляем со старой).
    cpu.setGravity(g1);
    gpu.setGravity(g1); // бампит cpuSceneVersion → ближайший updateAll re-upload'ит с g1

    for (int s = 0; s < kStepsAfter; ++s) {
        cpu.updateAll();
        gpu.updateAll();
        cpuStale.updateAll();
    }
    gpu.syncFromGpuIfNeeded();

    RuntimeResult r{};
    r.parity = maxAbsPositionDiff(cpu.atoms(), gpu.atoms());
    r.staleGapRef = maxAbsPositionDiff(cpu.atoms(), cpuStale.atoms());
    std::printf("[ WALLGRAV ] runtime : before=%d after=%d g0=(0,-3,0)->g1=(2,4,0) "
                "max|dCPU-GPU| = %.3e abs (stale-gap ref = %.3e)\n",
                kStepsBefore, kStepsAfter, r.parity, r.staleGapRef);
    return r;
}

void runWallGravityParity(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        const double staticMaxAbs = runStaticCase();
        const RuntimeResult rt = runRuntimeCase();

        state.counters["static_max_abs"] = staticMaxAbs;
        state.counters["runtime_max_abs"] = rt.parity;
        state.counters["runtime_stale_gap"] = rt.staleGapRef;

        // Tolerance (см. RESULTS.md провенанс): fp32-аккумуляция + разный порядок
        // суммирования (Half CPU vs Full GPU LJ; p2*p2*p2 wall) → дрейф в LSB-
        // диапазоне. Порог консервативный: < 1e-2 u за прогон (атомы движутся на
        // единицы u; это <1% движения). Кейс A — чистый паритет; кейс B несёт
        // одношаговый force-history transient от re-upload (force-буфера зануляются
        // на перезаливке) — для interior-сцены (только gravity) он мал.
        constexpr double kTol = 1e-2;
        if (staticMaxAbs > kTol) {
            throw std::runtime_error("GPU wall+gravity (static) diverged from CPU beyond tolerance");
        }
        if (rt.parity > kTol) {
            throw std::runtime_error("GPU runtime gravity-change diverged from CPU beyond tolerance");
        }
        // Sanity: смена gravity ДОЛЖНА быть заметной (иначе тест ничего не проверяет).
        // Если parity мал, а stale-gap тоже мал — gravity-change не повлияла, тест слеп.
        if (rt.staleGapRef <= kTol) {
            throw std::runtime_error("Runtime gravity-change had no measurable effect — test is blind");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuWallGravityParity/MatchesCpu","ru":"GPU wall+gravity == CPU","group":"Симуляция/GPU"}
void BM_GpuWallGravityParity_MatchesCpu(benchmark::State& state) { runWallGravityParity(state); }

BENCHMARK(BM_GpuWallGravityParity_MatchesCpu)->Unit(benchmark::kMillisecond);
