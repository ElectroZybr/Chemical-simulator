// Parity-гейт для GPU Full NeighborList build (Шаг 2b, shadow infra).
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Падение/throw абортит прогон — как прочие BM_Gpu*-гейты.
//
// Что проверяем (порядок соседей ВНУТРИ атома игнорируем — GPU обходит клетки в
// порядке стенсила, CPU тоже, но порядок несущественен; сравниваем как МНОЖЕСТВО):
//   1. На нескольких сценах строим CPU Full NeighborList (nl.build) И GPU NL.
//   2. Per-atom SET equality: множество соседей атома i на GPU == CPU Full NL.
//      missing==0 (нет потерянных пар) И extra==0 (нет лишних) для КАЖДОГО атома.
//   3. Symmetry (свойство Full): если j в списке i, то i в списке j.
//
// Сетка ОДНА на CPU и GPU: строим SpatialGrid у World (cellSize=6 >= listRadius=6),
// CPU NL ходит по ней, те же size/cellSize/countCells уходят в GPU builder —
// поэтому биннинг и 27-стенсил идентичны бит-в-бит (расхождение возможно только
// в fp-фильтре d2<=r^2, который и проверяет boundary-сцена).
//
// Любое расхождение -> throw с диагностикой (атом, missing/extra/asymmetry).

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
#include "Engine/physics/gpu/GpuNeighborListBuilder.h"
using namespace Lattice;

namespace {

// listRadius = cutoff(5)+skin(1) = 6, listRadiusSqr = 36. Совпадает с WorldState
// ctor (Simulation.cpp: setParams(5,1)) и resident GPU mode (setMode Full).
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

// --- генераторы сцен (cellSize=6, поэтому world кратен/около 6) ---

// Маленькая регулярная решётка: соседи предсказуемы (шаг 6 == cell), много пар
// ровно на границе r=6 — нагружает fp-фильтр d2<=36.
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

// Плотный кластер: много соседей у каждого атома (длинные слайсы NL, нагружает
// count/write по большому числу пар в одной клетке).
Scene makeDense() {
    Scene s{"dense-cluster", glm::vec3(120, 120, 120), {}, {}, {}};
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> d(20.0f, 32.0f); // ~2x2x2 клеток
    for (int i = 0; i < 4000; ++i) {
        addAtom(s, d(rng), d(rng), d(rng));
    }
    return s;
}

// Равномерный рандом по всему боксу.
Scene makeRandom(const char* name, int count, float box, uint32_t seed) {
    Scene s{name, glm::vec3(box, box, box), {}, {}, {}};
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> d(0.0f, box);
    for (int i = 0; i < count; ++i) {
        addAtom(s, d(rng), d(rng), d(rng));
    }
    return s;
}

// Около-граничные пары: пары на расстояниях около r=6 (внутри/снаружи/ровно) —
// прямой тест fp-фильтра d2<=36. Если GPU и CPU дадут разные вердикты на ровно
// граничной паре, gate это поймает (missing/extra != 0).
Scene makeBoundaryPairs() {
    Scene s{"boundary-pairs", glm::vec3(90, 90, 90), {}, {}, {}};
    // Пары вдоль оси X с разным разделением вокруг 6, на разных базовых позициях.
    const float bases[] = {12.0f, 30.0f, 48.0f};
    const float seps[] = {5.5f, 5.9f, 5.999f, 6.0f, 6.001f, 6.1f, 6.5f};
    for (float b : bases) {
        for (float sep : seps) {
            addAtom(s, b, b, b);
            addAtom(s, b + sep, b, b);
        }
    }
    // Плюс диагональные пары (d по 3 осям), чтобы проверить sqrt-границу не только по оси.
    for (float b : bases) {
        const float c = 3.4641f; // ~6/sqrt(3): d2 = 3*c^2 ~= 36 (около границы по диагонали)
        addAtom(s, b + 20.0f, b, b);
        addAtom(s, b + 20.0f + c, b + c, b + c);
    }
    return s;
}

// Атомы у стенок бокса: проверяет, что центр-клетка (ghost-clamp [1,size-2])
// совпадает CPU/GPU и обход 27 клеток у границы не уходит за пределы.
Scene makeNearWall() {
    Scene s{"near-wall", glm::vec3(90, 90, 90), {}, {}, {}};
    const float w = 90.0f;
    std::mt19937 rng(777);
    std::uniform_real_distribution<float> jit(-0.5f, 0.5f);
    // Слой атомов вдоль каждой стенки + чуть внутрь, со сгущением (соседство есть).
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

// Сцена «rebuild matters»: атомы на расстоянии чуть больше r друг от друга,
// смещённые так, что часть пар то входит, то выходит из листа — имитирует
// состояние, ради которого NL пересобирают. Для 2b важно, что GPU build даёт
// тот же набор, что свежий CPU build на тех же позициях.
Scene makeRebuildSensitive() {
    Scene s{"rebuild-sensitive", glm::vec3(120, 120, 120), {}, {}, {}};
    std::mt19937 rng(2024);
    std::uniform_real_distribution<float> d(0.0f, 120.0f);
    // Кластеры по ~30 атомов в радиусе ~r, разбросанные по боксу: много пар ровно
    // около границы листа.
    std::uniform_real_distribution<float> local(-5.5f, 5.5f);
    for (int c = 0; c < 40; ++c) {
        const float cx = d(rng), cy = d(rng), cz = d(rng);
        for (int k = 0; k < 30; ++k) {
            addAtom(s, cx + local(rng), cy + local(rng), cz + local(rng));
        }
    }
    return s;
}

// Большой рандом: countCells > одного scan-блока, и atomCount+1 даёт многоуровневый
// NL-scan (главная новая ветка 2b).
Scene makeLargeRandom() { return makeRandom("large-random", 60000, 300.0f, 999); }

// --- проверка одной сцены ---

void checkScene(GpuNeighborListBuilder& builder, const Scene& s) {
    const uint32_t atomCount = static_cast<uint32_t>(s.x.size());

    // CPU reference: World + grid (cellSize default 6) + Full NL.
    World world(s.world);
    AtomStorage& atoms = world.getAtomStorage();
    atoms.reserve(atomCount);
    for (uint32_t i = 0; i < atomCount; ++i) {
        atoms.addAtom(glm::vec3(s.x[i], s.y[i], s.z[i]), glm::vec3(0, 0, 0), AtomData::Type::H);
    }
    // Одна перестройка сетки (addAtom выше уже перестраивает, но делаем явно для
    // ясности и на случай, что atoms заполняли иначе).
    world.getGrid().rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());

    NeighborList& nl = world.getNeighborList();
    nl.setParams(kCutoff, kSkin);
    nl.setMode(NeighborListMode::Full);
    nl.build(atoms, world);

    const SpatialGrid& grid = world.getGrid();
    const uint32_t cellCount = static_cast<uint32_t>(grid.countCells);

    // GPU build (тот же grid размер/cellSize/countCells -> идентичный биннинг).
    builder.buildNeighborListFull(s.x, s.y, s.z, grid.size.x, grid.size.y, grid.size.z, grid.cellSize, cellCount, kListRadiusSqr);

    const std::vector<uint32_t> gpuOffsets = builder.readbackNlOffsets(); // длина atomCount+1
    if (gpuOffsets.size() != static_cast<size_t>(atomCount) + 1u) {
        std::printf("[FAIL] %s: gpu nlOffsets size %zu != atomCount+1 %u\n", s.name, gpuOffsets.size(), atomCount + 1u);
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuNeighborList: nlOffsets size mismatch");
    }
    const uint32_t gpuTotal = gpuOffsets[atomCount];
    const std::vector<uint32_t> gpuNeighbors = builder.readbackNlNeighbors(gpuTotal);

    // CPU CSR.
    const std::vector<uint32_t>& cpuOffsets = nl.offsets();   // atomCount+1
    const std::vector<uint32_t>& cpuNeighbors = nl.neighbors();
    if (cpuOffsets.size() != static_cast<size_t>(atomCount) + 1u) {
        std::printf("[FAIL] %s: cpu offsets size %zu != atomCount+1 %u\n", s.name, cpuOffsets.size(), atomCount + 1u);
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuNeighborList: cpu offsets size mismatch");
    }
    const uint32_t cpuTotal = cpuOffsets[atomCount];

    // Аккумуляторы расхождений по всем атомам.
    uint64_t totalMissing = 0; // в CPU есть, в GPU нет
    uint64_t totalExtra = 0;   // в GPU есть, в CPU нет
    uint64_t dupCount = 0;     // дубликаты в GPU-слайсе атома
    uint32_t firstBadAtom = 0xFFFFFFFFu;

    // Для symmetry-проверки собираем GPU-набор соседей как множества по атому.
    std::vector<std::unordered_set<uint32_t>> gpuSets(atomCount);

    for (uint32_t i = 0; i < atomCount; ++i) {
        const uint32_t gb = gpuOffsets[i];
        const uint32_t ge = gpuOffsets[i + 1];
        if (gb > ge || ge > gpuNeighbors.size()) {
            std::printf("[FAIL] %s: atom %u gpu slice [%u,%u) out of range (neighbors size %zu)\n", s.name, i, gb, ge,
                        gpuNeighbors.size());
            std::fflush(stdout);
            throw std::runtime_error("BM_GpuNeighborList: gpu slice out of range");
        }
        std::unordered_set<uint32_t>& gset = gpuSets[i];
        gset.reserve(ge - gb);
        for (uint32_t k = gb; k < ge; ++k) {
            const uint32_t j = gpuNeighbors[k];
            if (!gset.insert(j).second) {
                ++dupCount; // дубликат в слайсе атома i — баг записи
            }
        }

        // CPU-набор атома i.
        const uint32_t cb = cpuOffsets[i];
        const uint32_t ce = cpuOffsets[i + 1];
        std::unordered_set<uint32_t> cset;
        cset.reserve(ce - cb);
        for (uint32_t k = cb; k < ce; ++k) {
            cset.insert(cpuNeighbors[k]);
        }

        // missing: в CPU есть, в GPU нет.
        uint32_t miss = 0, extra = 0;
        for (uint32_t cj : cset) {
            if (gset.find(cj) == gset.end()) {
                ++miss;
            }
        }
        // extra: в GPU есть, в CPU нет.
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
        std::printf("[FAIL] %s: %llu duplicate neighbor entries in GPU slices\n", s.name,
                    static_cast<unsigned long long>(dupCount));
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuNeighborList: duplicate neighbor in GPU slice");
    }
    if (totalMissing != 0 || totalExtra != 0) {
        std::printf("[FAIL] %s: per-atom set mismatch missing=%llu extra=%llu firstBadAtom=%u (cpuTotal=%u gpuTotal=%u)\n", s.name,
                    static_cast<unsigned long long>(totalMissing), static_cast<unsigned long long>(totalExtra), firstBadAtom,
                    cpuTotal, gpuTotal);
        std::fflush(stdout);
        throw std::runtime_error("BM_GpuNeighborList: per-atom neighbor SET parity FAILED");
    }

    // Symmetry (Full): j в списке i => i в списке j. Проверяем на GPU-наборе
    // (он уже доказан равным CPU выше, но симметрия — независимый инвариант Full,
    // и его нарушение указывало бы на однонаправленную потерю записи).
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
        throw std::runtime_error("BM_GpuNeighborList: Full NL symmetry FAILED");
    }

    std::printf("[ NL-FULL ] %-18s atoms=%-6u cells=%-7u pairs(cpu=%u gpu=%u)  SET+SYM OK\n", s.name, atomCount, cellCount,
                cpuTotal, gpuTotal);
    std::fflush(stdout);
}

void runNeighborListParity(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        GpuNeighborListBuilder builder;
        // Несколько сцен через ОДИН builder: заодно проверяет рост буферов
        // (ensureCapacity / nlNeighbors grow) между разными размерами.
        checkScene(builder, makeSmallGrid());
        checkScene(builder, makeBoundaryPairs());
        checkScene(builder, makeNearWall());
        checkScene(builder, makeDense());
        checkScene(builder, makeRebuildSensitive());
        checkScene(builder, makeRandom("random-2k", 2000, 100.0f, 1));
        checkScene(builder, makeRandom("random-20k", 20000, 200.0f, 7));
        checkScene(builder, makeLargeRandom());
        // Прогон убывающего размера — буфера не должны переиспользоваться неверно.
        checkScene(builder, makeSmallGrid());
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuNeighborList/Parity","ru":"GPU Full NL parity vs CPU NeighborList","group":"Симуляция/GPU"}
void BM_GpuNeighborList_Parity(benchmark::State& state) { runNeighborListParity(state); }

BENCHMARK(BM_GpuNeighborList_Parity)->Iterations(1)->Unit(benchmark::kMicrosecond);
