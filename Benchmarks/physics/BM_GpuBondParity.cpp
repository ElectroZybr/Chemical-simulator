// Parity-гейт переноса Morse-сил связей на GPU (фаза 2.2a). GPU-траектория со
// статичными связями (Morse pair-силы) должна совпасть с CPU velocity Verlet
// (ForceField → Bond::forceBond) в пределах fp-tolerance. Без этого «GPU считает
// Morse-силы связей» — недоказанное утверждение.
//
// Это ВТОРАЯ под-стадия bond-сил: ТОЛЬКО Morse (par~force), БЕЗ угловых сил.
// Поэтому сцена — изолированные пары степени-1 (ни у одного атома нет ≥2 связей,
// значит угловой триплет не возникает, и угловое ядро 2.2b ничего бы не дало).
//
// Отличия и ключевые свойства гейта (требования дизайна §10):
//   1) GPU-сторона гоняется через РЕАЛЬНЫЙ путь Simulation::setGpuMode(true) +
//      updateAll() (а не голый GpuResidentPhysics), чтобы топология пришла из World
//      через uploadSceneToGpu → uploadBonds — тот же путь, что у приложения.
//   2) setLJEnabled(false) + setCoulombEnabled(false) на ОБЕИХ sim: иначе LJ/Кулон
//      двигали бы атомы, и гейт «прошёл» бы, не проверив bond-путь (Morse при
//      нулевой силе = 0; design §10 правка 1). GPU теперь чекает isLJEnabled и
//      пропускает compute_lj — без этого LJ молча вернулся бы на GPU.
//   3) Пары СМЕЩЕНЫ С РАВНОВЕСИЯ (C-C на 1.1 при r0=1.0; O-H на 0.9 при r0≈0.957):
//      Morse-сила НЕнулевая. + ASSERT, что CPU реально двинул атомы (иначе гейт слеп).
//   4) СМЕШАННЫЕ массы (O~16, C~12, H~1): invMass-зависимость correct'а развела бы
//      любую силовую ошибку (как gravity-как-сила в 2.1).
//   5) Резидентный CSR читается обратно (readbackBond*) и сверяется с CPU-adjacency
//      ТОЧНО — ловит «молча пустые связи» (ложный pass, как устаревшая gravity в 2.1).
//   6) Runtime-add: связь добавляется в GPU-режиме, один update, readback CSR
//      подтверждает прибытие новых directed-рёбер (Simulation::addBond бампит версию).
//   7) Начальное число связей не меняется за kSteps (расстояния << break 3.0) —
//      статичная топология; CPU-разрыв сделал бы расхождение из-за топологии, не силы.
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре.
// Бросает при расхождении/нарушении инварианта — прогон падает (как BM_GpuCorrectness).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <vector>

#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"

namespace {

constexpr float kDt = 0.01f;
constexpr float kAccelDamping = 0.9f;
constexpr int kCellSize = 6;
constexpr float kWorldSize = 24.0f; // max=23; атомы в районе центра, далеко от стен

// Геометрия пар (детерминированная, общая обеим sim). Степень-1 у каждого атома →
// углов нет. Пары разнесены >= cutoff(5) друг от друга, чтобы межпарных LJ-соседей
// не было (LJ всё равно выключен, но так сцена чище). Все расстояния << break 3.0.
//
// Пара C-C на 1.1 (r0=1.0): Morse притягивает (r > r0). Пара O-H на 0.9 (r0≈0.957):
// Morse отталкивает (r < r0). Разные знаки силы + разные массы → разнонаправленное
// движение, чувствительное к силовой ошибке.
struct PairSpec {
    AtomData::Type ta;
    AtomData::Type tb;
    Vec3f pa;
    Vec3f pb;
};

std::vector<PairSpec> pairSpecs() {
    return {
        // C-C, расстояние 1.1 по X (r0=1.0 → притяжение).
        {AtomData::Type::C, AtomData::Type::C, Vec3f{8.0f, 8.0f, 8.0f}, Vec3f{9.1f, 8.0f, 8.0f}},
        // O-H, расстояние 0.9 по X (r0≈0.957 → отталкивание).
        {AtomData::Type::O, AtomData::Type::H, Vec3f{8.0f, 14.0f, 8.0f}, Vec3f{8.9f, 14.0f, 8.0f}},
        // Ещё одна C-C на 1.1 в другом месте (больше directed-рёбер для CSR-сверки).
        {AtomData::Type::C, AtomData::Type::C, Vec3f{14.0f, 8.0f, 8.0f}, Vec3f{15.1f, 8.0f, 8.0f}},
    };
}

// Общая конфигурация sim: LJ/Кулон ВЫКЛ (изолируем Morse), формация ВЫКЛ, gravity=0.
void configureSim(Simulation& sim) {
    sim.createWorld(Vec3f{kWorldSize, kWorldSize, kWorldSize});
    sim.setSizeBox(sim.world().getWorldSize(), kCellSize);
    sim.setLJEnabled(false);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
    sim.setDt(kDt);
    sim.setAccelDamping(kAccelDamping);
}

// Заполняет sim атомами пар (порядок детерминирован: пара k → атомы 2k, 2k+1).
void fillPairs(Simulation& sim, const std::vector<PairSpec>& specs) {
    for (const PairSpec& s : specs) {
        sim.appendAtomFast(s.pa, Vec3f{0.0f, 0.0f, 0.0f}, s.ta, false);
        sim.appendAtomFast(s.pb, Vec3f{0.0f, 0.0f, 0.0f}, s.tb, false);
    }
    sim.finalizeAtomBatch();
}

// Создаёт связи пар (каждая пара (2k, 2k+1)). Порядок добавления детерминирован —
// он же задаёт ожидаемый порядок соседей в CSR.
void addPairBonds(Simulation& sim, size_t pairCount) {
    for (size_t k = 0; k < pairCount; ++k) {
        sim.addBond(2 * k, 2 * k + 1);
    }
}

// max|pos_a - pos_b| по всем атомам (порядок детерминирован — сцены идентичны).
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

// Ожидаемая bond-CSR из CPU-списка связей, зеркаля BondForceField.cpp:129-134 и
// uploadBonds: степени → prefix sum → fill двусторонним обходом в порядке списка.
struct ExpectedCsr {
    std::vector<uint32_t> offsets;   // длина n+1
    std::vector<uint32_t> neighbors; // длина 2*validBonds (directed edges)
};
ExpectedCsr buildExpectedCsr(const Bond::List& bonds, size_t n) {
    ExpectedCsr csr;
    csr.offsets.assign(n + 1, 0u);
    for (const Bond& b : bonds) {
        if (b.aIndex < n && b.bIndex < n) {
            ++csr.offsets[b.aIndex];
            ++csr.offsets[b.bIndex];
        }
    }
    uint32_t running = 0u;
    for (size_t i = 0; i < n; ++i) {
        const uint32_t deg = csr.offsets[i];
        csr.offsets[i] = running;
        running += deg;
    }
    csr.offsets[n] = running;
    csr.neighbors.assign(std::max<uint32_t>(running, 1u), 0xFFFFFFFFu);
    std::vector<uint32_t> cursor(n);
    for (size_t i = 0; i < n; ++i) {
        cursor[i] = csr.offsets[i];
    }
    for (const Bond& b : bonds) {
        if (b.aIndex < n && b.bIndex < n) {
            csr.neighbors[cursor[b.aIndex]++] = static_cast<uint32_t>(b.bIndex);
            csr.neighbors[cursor[b.bIndex]++] = static_cast<uint32_t>(b.aIndex);
        }
    }
    csr.neighbors.resize(running); // отбросить min-1 хвост при отсутствии связей
    return csr;
}

// Сверяет резидентный CSR (readback) с ожидаемым CPU-построением ТОЧНО.
// directedEdges == 2*validBonds; offsets длины n+1; per-atom порядок соседей совпал.
void assertCsrMatches(const GpuResidentPhysics& gpu, const Bond::List& bonds, size_t n, const char* label) {
    const ExpectedCsr expected = buildExpectedCsr(bonds, n);
    const uint32_t directedEdges = expected.offsets[n];

    const std::vector<uint32_t> gotOffsets = gpu.readbackBondOffsets();
    if (gotOffsets.size() != n + 1) {
        std::printf("[ BONDPAR  ] %s: offsets len %zu != totalCount+1 %zu\n", label, gotOffsets.size(), n + 1);
        throw std::runtime_error("BondParity: resident bondOffsets length mismatch");
    }
    if (gotOffsets[n] != directedEdges) {
        std::printf("[ BONDPAR  ] %s: directed edges %u != expected 2*bonds %u\n", label, gotOffsets[n], directedEdges);
        throw std::runtime_error("BondParity: resident directed-edge count mismatch");
    }
    for (size_t i = 0; i <= n; ++i) {
        if (gotOffsets[i] != expected.offsets[i]) {
            std::printf("[ BONDPAR  ] %s: offsets[%zu] %u != expected %u\n", label, i, gotOffsets[i], expected.offsets[i]);
            throw std::runtime_error("BondParity: resident bondOffsets content mismatch");
        }
    }

    const std::vector<uint32_t> gotNeighbors = gpu.readbackBondNeighbors(directedEdges);
    if (gotNeighbors.size() != expected.neighbors.size()) {
        throw std::runtime_error("BondParity: resident bondNeighbors length mismatch");
    }
    // Per-atom порядок: соседи каждого атома i в окне [offsets[i], offsets[i+1])
    // должны совпасть поэлементно (uploadBonds зеркалит CPU emplace_back-порядок).
    for (uint32_t k = 0; k < directedEdges; ++k) {
        if (gotNeighbors[k] != expected.neighbors[k]) {
            std::printf("[ BONDPAR  ] %s: neighbor[%u] %u != expected %u\n", label, k, gotNeighbors[k], expected.neighbors[k]);
            throw std::runtime_error("BondParity: resident bondNeighbors order/content mismatch");
        }
    }
}

struct BondParityResult {
    double cpuSelfDisplacement; // (a) CPU двинулся от стартовых позиций — Morse НЕнулев
    double maxAbs;              // (e) GPU vs CPU
    uint32_t directedEdgesBefore;
    uint32_t directedEdgesAfterRuntimeAdd; // (c)
    size_t cpuBondCountStart;
    size_t cpuBondCountEnd; // (d)
};

BondParityResult runBondParity() {
    constexpr int kSteps = 50;
    const std::vector<PairSpec> specs = pairSpecs();
    const size_t pairCount = specs.size();
    const size_t atomCount = 2 * pairCount;

    // Стартовые позиции (для assert'а (a): CPU реально двинулся).
    std::vector<Vec3f> startPos;
    startPos.reserve(atomCount);
    for (const PairSpec& s : specs) {
        startPos.push_back(s.pa);
        startPos.push_back(s.pb);
    }

    // --- CPU reference ---
    Simulation cpu;
    configureSim(cpu);
    fillPairs(cpu, specs);
    addPairBonds(cpu, pairCount);
    const size_t cpuBondCountStart = cpu.bonds().size();

    // --- GPU через реальный путь ---
    Simulation gpu;
    configureSim(gpu);
    fillPairs(gpu, specs);
    addPairBonds(gpu, pairCount);
    gpu.setGpuMode(true);

    // (b) Сразу после входа в GPU-режим: резидентный CSR == CPU-adjacency.
    {
        const GpuResidentPhysics* res = gpu.activeGpuResident();
        if (res == nullptr) {
            throw std::runtime_error("BondParity: GPU resident missing after setGpuMode");
        }
        assertCsrMatches(*res, gpu.bonds(), atomCount, "after setGpuMode");
    }
    const uint32_t directedEdgesBefore = 2u * static_cast<uint32_t>(cpuBondCountStart);

    // Прогон обеих сторон.
    for (int s = 0; s < kSteps; ++s) {
        cpu.updateAll();
        gpu.updateAll();
    }
    gpu.syncFromGpuIfNeeded(); // стянуть резидентные позиции в CPU-копию для сверки

    // (a) CPU реально двинулся от стартовых позиций (Morse НЕнулева — гейт не слеп).
    double cpuSelfDisplacement = 0.0;
    {
        const AtomStorage& a = cpu.atoms();
        for (size_t i = 0; i < atomCount; ++i) {
            const Vec3f sp = startPos[i];
            cpuSelfDisplacement = std::max(cpuSelfDisplacement, std::abs(static_cast<double>(a.posX(i)) - sp.x));
            cpuSelfDisplacement = std::max(cpuSelfDisplacement, std::abs(static_cast<double>(a.posY(i)) - sp.y));
            cpuSelfDisplacement = std::max(cpuSelfDisplacement, std::abs(static_cast<double>(a.posZ(i)) - sp.z));
        }
    }

    // (d) Начальные связи не порвались (статичная топология).
    const size_t cpuBondCountEnd = cpu.bonds().size();

    // (e) GPU vs CPU max|Δpos|.
    const double maxAbs = maxAbsPositionDiff(cpu.atoms(), gpu.atoms());

    // (c) Runtime-add: добавить новую связь в GPU-режиме (новая пара атомов),
    // один update → re-upload подхватит → readback подтверждает новые рёбра.
    // Добавляем новую C-C пару в свободном месте; их valence > 0 (свежие атомы).
    gpu.appendAtomFast(Vec3f{8.0f, 8.0f, 14.0f}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::C, false);
    gpu.appendAtomFast(Vec3f{9.1f, 8.0f, 14.0f}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::C, false);
    gpu.finalizeAtomBatch();
    const size_t newA = gpu.atoms().size() - 2;
    const size_t newB = gpu.atoms().size() - 1;
    gpu.addBond(newA, newB); // бампит cpuSceneVersion → ближайший update re-upload'ит CSR
    gpu.updateAll();
    gpu.syncFromGpuIfNeeded();

    uint32_t directedEdgesAfterRuntimeAdd = 0;
    {
        const GpuResidentPhysics* res = gpu.activeGpuResident();
        if (res == nullptr) {
            throw std::runtime_error("BondParity: GPU resident missing after runtime add");
        }
        const size_t nAfter = gpu.atoms().size();
        // CSR резидента должен совпасть с НОВЫМ CPU-списком (старые + новая связь).
        assertCsrMatches(*res, gpu.bonds(), nAfter, "after runtime add");
        directedEdgesAfterRuntimeAdd = res->readbackBondOffsets()[nAfter];
    }

    BondParityResult r{cpuSelfDisplacement, maxAbs, directedEdgesBefore, directedEdgesAfterRuntimeAdd,
                       cpuBondCountStart,    cpuBondCountEnd};
    std::printf("[ BONDPAR  ] morse-only: pairs=%zu steps=%d CPU-self-disp=%.3e max|dCPU-GPU|=%.3e abs "
                "(edges %u->%u, bonds %zu->%zu)\n",
                pairCount, kSteps, r.cpuSelfDisplacement, r.maxAbs, r.directedEdgesBefore,
                r.directedEdgesAfterRuntimeAdd, r.cpuBondCountStart, r.cpuBondCountEnd);
    return r;
}

// Регрессия на доставку РАНТАЙМ-смены setLJEnabled в GPU-режим. GPU-шаг диспатчит
// LJ под флагом ljEnabled_, снятым на upload (GpuResidentPhysics.cpp). Если
// setLJEnabled НЕ бампит cpuSceneVersion, выключение LJ ПОСЛЕ setGpuMode(true) не
// долетает до GPU (step продолжает считать LJ по устаревшему флагу) → дивергенция с
// CPU. Сцена: 2 близких H (только LJ, без связей/стен), LJ ВКЛ старт; прогон, потом
// setLJEnabled(false) на ОБЕИХ sim, прогон, сверка. Эталон cpuStale (LJ остаётся ВКЛ)
// даёт масштаб разрыва — если бы GPU проигнорил toggle, он повёл бы себя как cpuStale.
struct LJToggleResult {
    double parity;      // GPU(toggle) vs CPU(toggle) — должно быть мало (toggle долетел)
    double staleGapRef; // CPU(toggle) vs CPU(LJ остался ВКЛ) — масштаб «если бы GPU проигнорил»
};
LJToggleResult runLJToggleDelivery() {
    constexpr int kBefore = 10;
    constexpr int kAfter = 30;
    // Стабильная LJ-решётка (как BM_GpuCorrectness): 3x3x3 H, spacing 3.0, малые
    // seeded-скорости, атомы глубоко внутри box. LJ активна (двигает атомы по-разному
    // вкл/выкл) и устойчива. (2 близких атома с сильным LJ дали бы NaN-позиции →
    // мусорный индекс ячейки в биннинге → out-of-bounds краш; решётка spacing=3 этого
    // избегает.) Toggle LJ заметно меняет траекторию → staleGapRef > tol.
    auto build = [](Simulation& sim) {
        constexpr float spacing = 3.0f;
        constexpr int side = 3;
        constexpr float worldSize = side * spacing + 40.0f;
        sim.createWorld(Vec3f{worldSize, worldSize, worldSize});
        sim.setSizeBox(sim.world().getWorldSize(), 6);
        sim.setLJEnabled(true); // LJ ВКЛ на старте — потом выключим в рантайме
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
        sim.setDt(kDt);
        sim.setAccelDamping(kAccelDamping);
        std::mt19937 rng(424242);
        std::uniform_real_distribution<float> vel(-0.3f, 0.3f);
        for (int z = 0; z < side; ++z) {
            for (int y = 0; y < side; ++y) {
                for (int x = 0; x < side; ++x) {
                    sim.appendAtomFast(Vec3f{20.0f + x * spacing, 20.0f + y * spacing, 20.0f + z * spacing},
                                       Vec3f{vel(rng), vel(rng), vel(rng)}, AtomData::Type::H, false);
                }
            }
        }
        sim.finalizeAtomBatch();
    };
    Simulation cpu;
    build(cpu);
    Simulation gpu;
    build(gpu);
    gpu.setGpuMode(true);
    Simulation cpuStale; // эталон: LJ НЕ выключаем
    build(cpuStale);

    for (int s = 0; s < kBefore; ++s) {
        cpu.updateAll();
        gpu.updateAll();
        cpuStale.updateAll();
    }
    cpu.setLJEnabled(false);
    gpu.setLJEnabled(false); // с фиксом: бампит версию → ближайший update re-upload'ит ljEnabled_=false
    for (int s = 0; s < kAfter; ++s) {
        cpu.updateAll();
        gpu.updateAll();
        cpuStale.updateAll();
    }
    gpu.syncFromGpuIfNeeded();

    LJToggleResult r{};
    r.parity = maxAbsPositionDiff(cpu.atoms(), gpu.atoms());
    r.staleGapRef = maxAbsPositionDiff(cpu.atoms(), cpuStale.atoms());
    std::printf("[ BONDPAR  ] lj-toggle : before=%d after=%d max|dCPU-GPU|=%.3e abs (stale-gap ref=%.3e)\n", kBefore,
                kAfter, r.parity, r.staleGapRef);
    return r;
}

void runBondParityGate(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        const BondParityResult r = runBondParity();
        const LJToggleResult lj = runLJToggleDelivery();

        state.counters["max_abs"] = r.maxAbs;
        state.counters["cpu_self_disp"] = r.cpuSelfDisplacement;
        state.counters["directed_edges"] = r.directedEdgesBefore;

        // (a) CPU реально двинулся → Morse-сила НЕнулева. Без этого 0==0 «прошло» бы.
        constexpr double kNonZeroDisp = 1e-4;
        if (r.cpuSelfDisplacement <= kNonZeroDisp) {
            throw std::runtime_error("BondParity: CPU Morse produced ~0 displacement — gate is blind (force is zero)");
        }
        // (c) Runtime-add: добавилась РОВНО одна связь → +2 directed-ребра.
        if (r.directedEdgesAfterRuntimeAdd != r.directedEdgesBefore + 2u) {
            throw std::runtime_error("BondParity: runtime-added bond did not reach VRAM (directed edges unchanged)");
        }
        // (d) Начальные связи не порвались (статичная топология за прогон).
        if (r.cpuBondCountEnd != r.cpuBondCountStart) {
            throw std::runtime_error("BondParity: CPU broke a bond during run — scene invalid (topology changed)");
        }
        // (e) GPU vs CPU. Tolerance: fp32-аккумуляция + CPU считает геометрию forceBond
        // в double, Morse во float, GPU всё в f32 → дрейф в LSB-диапазоне. Порог
        // стартовый 1e-2 (как BM_GpuCorrectness; провенанс в RESULTS.md). Morse-only
        // (без acos/angle) ожидаемо укладывается в этот порог.
        constexpr double kTol = 1e-2;
        if (r.maxAbs > kTol) {
            throw std::runtime_error("GPU Morse-bond trajectory diverged from CPU beyond tolerance");
        }

        state.counters["lj_toggle_parity"] = lj.parity;
        state.counters["lj_toggle_stale_gap"] = lj.staleGapRef;
        // Рантайм-смена setLJEnabled долетела до GPU (фикс: setLJEnabled бампит версию).
        // Доставлено ⟺ GPU(LJ выкл) НАМНОГО ближе к CPU(LJ выкл), чем масштаб самого
        // эффекта toggle (staleGapRef). Если бы не долетела, GPU держал бы LJ → parity
        // ≈ staleGapRef. Порог ОТНОСИТЕЛЬНЫЙ (не абсолютный kTol): эффект toggle здесь
        // ~1e-3, и абсолютный 1e-2 не отличил бы доставку от игнора. Форма !(parity < ...)
        // ловит и parity=NaN (если бы GPU взорвался).
        if (!(lj.parity < 0.25 * lj.staleGapRef)) {
            throw std::runtime_error("GPU did not honor runtime setLJEnabled(false) — parity not << toggle effect");
        }
        // Тест не слеп: выключение LJ реально изменило траекторию заметно выше fp-шума паритета.
        if (lj.staleGapRef < 5e-4) {
            throw std::runtime_error("LJ toggle had no measurable effect — test is blind (LJ inactive over run)");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuBondParity/MorseMatchesCpu","ru":"GPU Morse-связи == CPU","group":"Симуляция/GPU"}
void BM_GpuBondParity_MorseMatchesCpu(benchmark::State& state) { runBondParityGate(state); }

BENCHMARK(BM_GpuBondParity_MorseMatchesCpu)->Unit(benchmark::kMillisecond);
