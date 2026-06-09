// Parity-гейт для GPU NL build В РЕЗИДЕНТНЫЕ буфера (Шаг 2c).
//
// Отличие от BM_GpuNeighborList (2b): там builder проверяется по его СОБСТВЕННЫМ
// shadow-буферам. Здесь поднимается реальный GpuResidentPhysics (через
// uploadFromCpu, как BM_GpuCorrectness), вызывается rebuildNeighborListOnGpu(), и
// читаются именно РЕЗИДЕНТНЫЕ nlOffsets_/nlNeighbors_ — те, что читает LJ-ядро.
// Это доказывает, что GPU-rebuild оставляет в hot-loop-буферах корректный NL,
// а не только в тени builder'а.
//
// Что проверяем (порядок соседей ВНУТРИ атома игнорируем — сравниваем как МНОЖЕСТВО):
//   1. На нескольких сценах строим CPU Full NeighborList И GPU-resident NL.
//   2. Per-atom SET equality: множество соседей атома i в РЕЗИДЕНТНОМ NL == CPU Full.
//      missing==0 (нет потерянных пар) И extra==0 (нет лишних) для КАЖДОГО атома.
//   3. Symmetry (свойство Full): если j в списке i, то i в списке j.
//   4. Одна сцена ЯВНО форсирует capacity-grow: резидентный nlNeighbors_ заводится
//      под крошечный начальный NL (tiny listRadius при upload), а rebuild с реальным
//      r=6 даёт total >> ёмкости → fail-closed grow должен вырасти и НЕ потерять пары.
//
// Сетка ОДНА на CPU и GPU (cellSize=6 >= listRadius=6): grid у World, CPU NL ходит
// по ней, те же size/cellSize/countCells уходят в GPU builder через resident-метод.
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Падение/throw абортит прогон — как прочие BM_Gpu*-гейты.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/World.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"
using namespace Lattice;

namespace {

// listRadius = cutoff(5)+skin(1) = 6, listRadiusSqr = 36. Совпадает с resident GPU
// mode (setMode Full) и BM_GpuNeighborList.
constexpr float kCutoff = 5.0f;
constexpr float kSkin = 1.0f;
constexpr float kListRadiusSqr = (kCutoff + kSkin) * (kCutoff + kSkin); // 36

struct Scene {
    const char* name;
    glm::vec3 world;
    std::vector<float> x, y, z;
};

void addAtom(Scene& s, float px, float py, float pz) {
    s.x.push_back(px);
    s.y.push_back(py);
    s.z.push_back(pz);
}

// --- генераторы сцен (cellSize=6) — те же формы, что BM_GpuNeighborList ---

Scene makeSmallGrid() {
    Scene s{"small-grid", glm::vec3(60, 60, 60), {}, {}, {}};
    for (int i = 0; i < 64; ++i) {
        const float px = 6.0f + 6.0f * static_cast<float>(i % 4);
        const float py = 6.0f + 6.0f * static_cast<float>((i / 4) % 4);
        const float pz = 6.0f + 6.0f * static_cast<float>(i / 16);
        addAtom(s, px, py, pz);
    }
    return s;
}

Scene makeDense() {
    Scene s{"dense-cluster", glm::vec3(120, 120, 120), {}, {}, {}};
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> d(20.0f, 32.0f); // ~2x2x2 клеток
    for (int i = 0; i < 4000; ++i) {
        addAtom(s, d(rng), d(rng), d(rng));
    }
    return s;
}

Scene makeRandom(const char* name, int count, float box, uint32_t seed) {
    Scene s{name, glm::vec3(box, box, box), {}, {}, {}};
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> d(0.0f, box);
    for (int i = 0; i < count; ++i) {
        addAtom(s, d(rng), d(rng), d(rng));
    }
    return s;
}

Scene makeBoundaryPairs() {
    Scene s{"boundary-pairs", glm::vec3(90, 90, 90), {}, {}, {}};
    const float bases[] = {12.0f, 30.0f, 48.0f};
    const float seps[] = {5.5f, 5.9f, 5.999f, 6.0f, 6.001f, 6.1f, 6.5f};
    for (float b : bases) {
        for (float sep : seps) {
            addAtom(s, b, b, b);
            addAtom(s, b + sep, b, b);
        }
    }
    for (float b : bases) {
        const float c = 3.4641f; // ~6/sqrt(3)
        addAtom(s, b + 20.0f, b, b);
        addAtom(s, b + 20.0f + c, b + c, b + c);
    }
    return s;
}

Scene makeNearWall() {
    Scene s{"near-wall", glm::vec3(90, 90, 90), {}, {}, {}};
    const float w = 90.0f;
    std::mt19937 rng(777);
    std::uniform_real_distribution<float> jit(-0.5f, 0.5f);
    const float walls[] = {0.5f, 2.0f, 4.0f, w - 4.0f, w - 2.0f, w - 0.5f};
    for (float wx : walls) {
        for (float fy = 4.0f; fy < w; fy += 4.0f) {
            for (float fz = 4.0f; fz < w; fz += 4.0f) {
                addAtom(s, wx + jit(rng), fy + jit(rng), fz + jit(rng));
            }
        }
    }
    return s;
}

// Заполняет World атомами сцены + строит grid. Возвращает ничего — мутирует world.
void fillWorld(World& world, const Scene& s) {
    AtomStorage& atoms = world.getAtomStorage();
    const uint32_t atomCount = static_cast<uint32_t>(s.x.size());
    atoms.reserve(atomCount);
    for (uint32_t i = 0; i < atomCount; ++i) {
        atoms.addAtom(glm::vec3(s.x[i], s.y[i], s.z[i]), glm::vec3(0, 0, 0), AtomData::Type::H);
    }
    world.getGrid().rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());
}

// Сравнение РЕЗИДЕНТНОГО NL с CPU Full NL. forceCapacityGrow=true: резидентный
// nlNeighbors_ заводится под крошечный начальный NL (tiny listRadius при upload),
// тогда rebuild с реальным r=6 обязан вырасти (fail-closed grow) — проверяем, что
// grow реально случился и пары не потеряны.
void checkScene(const Scene& s, bool forceCapacityGrow) {
    const uint32_t atomCount = static_cast<uint32_t>(s.x.size());

    // --- CPU reference: World + grid (cellSize 6) + Full NL на реальном r=6 ---
    World cpuWorld(s.world);
    fillWorld(cpuWorld, s);
    NeighborList& cpuNl = cpuWorld.getNeighborList();
    cpuNl.setParams(kCutoff, kSkin);
    cpuNl.setMode(NeighborListMode::Full);
    cpuNl.build(cpuWorld.getAtomStorage(), cpuWorld);

    const SpatialGrid& grid = cpuWorld.getGrid();
    const uint32_t cellCount = static_cast<uint32_t>(grid.countCells);

    // --- GPU resident: тот же стартовый стейт ---
    World gpuWorld(s.world);
    fillWorld(gpuWorld, s);
    NeighborList& seedNl = gpuWorld.getNeighborList();
    seedNl.setMode(NeighborListMode::Full);
    if (forceCapacityGrow) {
        // Крошечный listRadius -> почти нет соседей -> крошечная резидентная ёмкость
        // nlNeighbors_. Так последующий rebuild с реальным r=6 форсирует grow.
        // (cellSize=6 >= listRadius тривиально для tiny radius.)
        seedNl.setParams(0.1f, 0.0f);
    } else {
        seedNl.setParams(kCutoff, kSkin);
    }
    seedNl.build(gpuWorld.getAtomStorage(), gpuWorld);

    LJForceField lj;
    GpuResidentPhysics grp;
    grp.uploadFromCpu(gpuWorld.getAtomStorage(), seedNl, lj, s.world.x, s.world.y, s.world.z, 0.0f, 0.0f, 0.0f,
                      /*ljEnabled=*/true, /*coulombEnabled=*/false); // gravity=0 (NL-build гейт)

    const uint64_t growsBefore = grp.nlCapacityGrows();

    // GPU NL build В РЕЗИДЕНТНЫЕ буфера (тот же grid размер/cellSize/countCells, тот
    // же реальный r=6, что и CPU reference -> идентичный биннинг и фильтр).
    grp.rebuildNeighborListOnGpu(grid.size.x, grid.size.y, grid.size.z, grid.cellSize, cellCount, kListRadiusSqr);

    const uint64_t growsAfter = grp.nlCapacityGrows();

    // Читаем именно РЕЗИДЕНТНЫЕ буфера (те, что читает LJ-ядро).
    const std::vector<uint32_t> gpuOffsets = grp.readbackNlOffsets(); // длина atomCount+1
    if (gpuOffsets.size() != static_cast<size_t>(atomCount) + 1u) {
        std::printf("[FAIL] %s: resident nlOffsets size %zu != atomCount+1 %u\n", s.name, gpuOffsets.size(), atomCount + 1u);
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuResidentNlBuild: resident nlOffsets size mismatch");
    }
    const uint32_t gpuTotal = gpuOffsets[atomCount];
    const std::vector<uint32_t> gpuNeighbors = grp.readbackNlNeighbors(gpuTotal);

    // CPU CSR.
    const std::vector<uint32_t>& cpuOffsets = cpuNl.offsets(); // atomCount+1
    const std::vector<uint32_t>& cpuNeighbors = cpuNl.neighbors();
    if (cpuOffsets.size() != static_cast<size_t>(atomCount) + 1u) {
        std::printf("[FAIL] %s: cpu offsets size %zu != atomCount+1 %u\n", s.name, cpuOffsets.size(), atomCount + 1u);
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuResidentNlBuild: cpu offsets size mismatch");
    }
    const uint32_t cpuTotal = cpuOffsets[atomCount];

    // Если форсировали grow — он ОБЯЗАН был случиться, иначе сцена не нагрузила
    // overflow-путь (гейт стал бы ложно-зелёным на нём).
    if (forceCapacityGrow && growsAfter == growsBefore) {
        std::printf("[FAIL] %s: capacity grow expected but nlCapacityGrows() unchanged (=%llu); gpuTotal=%u — сцена не "
                    "форсировала overflow\n",
                    s.name, static_cast<unsigned long long>(growsAfter), gpuTotal);
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuResidentNlBuild: expected capacity grow did not happen");
    }

    // Аккумуляторы расхождений по всем атомам.
    uint64_t totalMissing = 0; // в CPU есть, в GPU нет
    uint64_t totalExtra = 0;   // в GPU есть, в CPU нет
    uint64_t dupCount = 0;     // дубликаты в GPU-слайсе атома
    uint32_t firstBadAtom = 0xFFFFFFFFu;

    std::vector<std::unordered_set<uint32_t>> gpuSets(atomCount);

    for (uint32_t i = 0; i < atomCount; ++i) {
        const uint32_t gb = gpuOffsets[i];
        const uint32_t ge = gpuOffsets[i + 1];
        if (gb > ge || ge > gpuNeighbors.size()) {
            std::printf("[FAIL] %s: atom %u resident slice [%u,%u) out of range (neighbors size %zu)\n", s.name, i, gb, ge,
                        gpuNeighbors.size());
            std::fflush(stdout);
            throw std::runtime_error("BM_GpuResidentNlBuild: resident slice out of range");
        }
        std::unordered_set<uint32_t>& gset = gpuSets[i];
        gset.reserve(ge - gb);
        for (uint32_t k = gb; k < ge; ++k) {
            const uint32_t j = gpuNeighbors[k];
            if (!gset.insert(j).second) {
                ++dupCount;
            }
        }

        const uint32_t cb = cpuOffsets[i];
        const uint32_t ce = cpuOffsets[i + 1];
        std::unordered_set<uint32_t> cset;
        cset.reserve(ce - cb);
        for (uint32_t k = cb; k < ce; ++k) {
            cset.insert(cpuNeighbors[k]);
        }

        uint32_t miss = 0, extra = 0;
        for (uint32_t cj : cset) {
            if (gset.find(cj) == gset.end()) {
                ++miss;
            }
        }
        for (uint32_t gj : gset) {
            if (cset.find(gj) == cset.end()) {
                ++extra;
            }
        }
        if ((miss != 0 || extra != 0) && firstBadAtom == 0xFFFFFFFFu) {
            firstBadAtom = i;
            std::printf("[FAIL] %s: atom %u set differs missing=%u extra=%u (cpu n=%u gpu n=%u)\n", s.name, i, miss, extra,
                        ce - cb, ge - gb);
        }
        totalMissing += miss;
        totalExtra += extra;
    }

    if (dupCount != 0) {
        std::printf("[FAIL] %s: %llu duplicate neighbor entries in resident slices\n", s.name,
                    static_cast<unsigned long long>(dupCount));
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuResidentNlBuild: duplicate neighbor in resident slice");
    }
    if (totalMissing != 0 || totalExtra != 0) {
        std::printf("[FAIL] %s: per-atom set mismatch missing=%llu extra=%llu firstBadAtom=%u (cpuTotal=%u gpuTotal=%u)\n", s.name,
                    static_cast<unsigned long long>(totalMissing), static_cast<unsigned long long>(totalExtra), firstBadAtom,
                    cpuTotal, gpuTotal);
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuResidentNlBuild: resident per-atom neighbor SET parity FAILED");
    }

    // Symmetry (Full): j в списке i => i в списке j.
    uint64_t asym = 0;
    uint32_t firstAsymI = 0xFFFFFFFFu, firstAsymJ = 0;
    for (uint32_t i = 0; i < atomCount; ++i) {
        for (uint32_t j : gpuSets[i]) {
            if (j >= atomCount || gpuSets[j].find(i) == gpuSets[j].end()) {
                if (asym == 0) {
                    firstAsymI = i;
                    firstAsymJ = j;
                }
                ++asym;
            }
        }
    }
    if (asym != 0) {
        std::printf("[FAIL] %s: %llu asymmetric pairs (first i=%u j=%u)\n", s.name, static_cast<unsigned long long>(asym),
                    firstAsymI, firstAsymJ);
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuResidentNlBuild: Full NL symmetry FAILED");
    }

    std::printf("[ RESIDENT-NL ] %-18s atoms=%-6u cells=%-7u pairs(cpu=%u gpu=%u)  grows=%llu  SET+SYM OK%s\n", s.name, atomCount,
                cellCount, cpuTotal, gpuTotal, static_cast<unsigned long long>(growsAfter - growsBefore),
                forceCapacityGrow ? " [grow forced]" : "");
    std::fflush(stdout);
}

void runResidentNlParity(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        // Обычные сцены: резидентный NL == CPU Full на тех же позициях.
        checkScene(makeSmallGrid(), /*forceCapacityGrow=*/false);
        checkScene(makeBoundaryPairs(), false);
        checkScene(makeNearWall(), false);
        checkScene(makeDense(), false);
        checkScene(makeRandom("random-2k", 2000, 100.0f, 1), false);
        checkScene(makeRandom("random-20k", 20000, 200.0f, 7), false);
        // Capacity-grow: плотная сцена с крошечной начальной ёмкостью -> overflow
        // fail-closed grow обязателен и пары не должны потеряться.
        checkScene(makeDense(), /*forceCapacityGrow=*/true);
        checkScene(makeRandom("random-20k-grow", 20000, 200.0f, 7), /*forceCapacityGrow=*/true);
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuResidentNlBuild/Parity","ru":"GPU resident NL parity vs CPU NeighborList","group":"Симуляция/GPU"}
void BM_GpuResidentNlBuild_Parity(benchmark::State& state) { runResidentNlParity(state); }

BENCHMARK(BM_GpuResidentNlBuild_Parity)->Iterations(1)->Unit(benchmark::kMicrosecond);
