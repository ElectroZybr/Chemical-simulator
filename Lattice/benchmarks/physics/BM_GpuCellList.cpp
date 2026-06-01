// Parity-гейт для GPU counting-sort cell-list (Шаг 2a, shadow infra).
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Падение/throw абортит прогон — как прочие BM_Gpu*-гейты.
//
// Что проверяем (порядок ВНУТРИ клетки игнорируем — atomic scatter недетерминирован):
//   1. scan parity: GPU cellCounts == CPU per-cell counts; GPU cellOffsets ==
//      CPU reference exclusive scan тех же counts.
//   2. per-cell SET equality: множество атомов в каждой клетке GPU == CPU
//      SpatialGrid::atomsInCell(c).
//
// Если хоть одна клетка расходится — throw с диагностикой (cell, gpu vs cpu).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/gpu/GpuNeighborListBuilder.h"
using namespace Lattice;

namespace {

struct Scene {
    const char* name;
    Vec3f world;
    float cellSize;
    std::vector<float> x, y, z;
};

void addAtom(Scene& s, float px, float py, float pz) {
    s.x.push_back(px);
    s.y.push_back(py);
    s.z.push_back(pz);
}

// --- генераторы сцен ---

// Маленькая регулярная решётка.
Scene makeSmallGrid() {
    Scene s{"small-grid", Vec3f(60, 60, 60), 6.0f, {}, {}, {}};
    for (int i = 0; i < 64; ++i) {
        const float px = 6.0f + 6.0f * static_cast<float>(i % 4);
        const float py = 6.0f + 6.0f * static_cast<float>((i / 4) % 4);
        const float pz = 6.0f + 6.0f * static_cast<float>(i / 16);
        addAtom(s, px, py, pz);
    }
    return s;
}

// Плотный кластер: много атомов в малом объёме (несколько клеток по многу атомов
// -> нагружает atomic scatter и счётчики клеток).
Scene makeDense() {
    Scene s{"dense-cluster", Vec3f(120, 120, 120), 6.0f, {}, {}, {}};
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> d(20.0f, 32.0f); // ~2x2x2 клеток (cell 6)
    for (int i = 0; i < 4000; ++i) {
        addAtom(s, d(rng), d(rng), d(rng));
    }
    return s;
}

// Равномерный рандом по всему боксу.
Scene makeRandom(int count, uint32_t seed) {
    Scene s{"random", Vec3f(300, 300, 300), 6.0f, {}, {}, {}};
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> d(0.0f, 300.0f);
    for (int i = 0; i < count; ++i) {
        addAtom(s, d(rng), d(rng), d(rng));
    }
    return s;
}

// Около-граничные атомы: ровно на 0, на стенке, чуть за стенкой (тест clamp в
// ghost-интерьер [1, size-2]) + немного внутри.
Scene makeBoundary() {
    Scene s{"boundary", Vec3f(90, 90, 90), 6.0f, {}, {}, {}};
    const float w = 90.0f;
    const float pts[] = {-3.0f, 0.0f, 0.001f, 3.0f, w * 0.5f, w - 0.001f, w, w + 3.0f};
    for (float px : pts) {
        for (float py : pts) {
            for (float pz : pts) {
                addAtom(s, px, py, pz);
            }
        }
    }
    return s;
}

// Большой рандом: countCells заведомо > одного scan-блока (2 уровня
// иерархического scan). 300^3 бокс, cell 6 -> ~52^3 ~= 140k клеток
// (140608/512 = 275 блоков -> 2-й уровень).
Scene makeLargeRandom() { return makeRandom(60000, 999); }

// Очень большая сетка: countCells > 512*512 = 262144 -> ТРИ уровня scan
// (cellCount -> numBlocks0 -> numBlocks1 -> 1). Прямо нагружает самую глубокую
// ветку рекурсии scan (главный риск 2a). 420^3, cell 6 -> ~72^3 ~= 373k клеток.
Scene makeHugeGrid() {
    Scene s{"huge-grid", Vec3f(420, 420, 420), 6.0f, {}, {}, {}};
    std::mt19937 rng(31337);
    std::uniform_real_distribution<float> d(0.0f, 420.0f);
    for (int i = 0; i < 30000; ++i) {
        addAtom(s, d(rng), d(rng), d(rng));
    }
    return s;
}

// --- проверка одной сцены ---

void checkScene(GpuNeighborListBuilder& builder, const Scene& s) {
    // CPU reference.
    SpatialGrid grid(s.world, s.cellSize);
    grid.rebuild(s.x, s.y, s.z);

    const uint32_t cellCount = static_cast<uint32_t>(grid.countCells);
    const uint32_t atomCount = static_cast<uint32_t>(s.x.size());

    // CPU per-cell counts + reference exclusive scan.
    std::vector<uint32_t> cpuCounts(cellCount, 0);
    for (uint32_t c = 0; c < cellCount; ++c) {
        cpuCounts[c] = static_cast<uint32_t>(grid.atomsInCell(c).size());
    }
    std::vector<uint32_t> cpuScan(cellCount, 0);
    uint32_t running = 0;
    for (uint32_t c = 0; c < cellCount; ++c) {
        cpuScan[c] = running;
        running += cpuCounts[c];
    }
    if (running != atomCount) {
        std::printf("[FAIL] %s: CPU scan total %u != atomCount %u\n", s.name, running, atomCount);
        throw std::runtime_error("BM_GpuCellList: CPU reference scan total mismatch");
    }

    // GPU build + readback.
    builder.build(s.x, s.y, s.z, grid.size.x, grid.size.y, grid.size.z, grid.cellSize, cellCount);
    const std::vector<uint32_t> gpuCounts = builder.readbackCellCounts();
    const std::vector<uint32_t> gpuOffsets = builder.readbackCellOffsets();
    const std::vector<uint32_t> gpuAtoms = builder.readbackAtomsInCells();

    if (gpuCounts.size() != cellCount || gpuOffsets.size() != cellCount) {
        throw std::runtime_error("BM_GpuCellList: GPU readback size mismatch");
    }

    // 1) scan parity: counts и offsets бит-в-бит.
    uint32_t countMismatch = 0, scanMismatch = 0;
    uint32_t firstBadCell = 0xFFFFFFFFu;
    for (uint32_t c = 0; c < cellCount; ++c) {
        if (gpuCounts[c] != cpuCounts[c]) {
            if (countMismatch == 0) firstBadCell = c;
            ++countMismatch;
        }
        if (gpuOffsets[c] != cpuScan[c]) {
            if (scanMismatch == 0 && firstBadCell == 0xFFFFFFFFu) firstBadCell = c;
            ++scanMismatch;
        }
    }
    if (countMismatch != 0 || scanMismatch != 0) {
        std::printf("[FAIL] %s: count_mismatch=%u scan_mismatch=%u firstCell=%u (gpuCount=%u cpuCount=%u gpuOff=%u cpuScan=%u)\n",
                    s.name, countMismatch, scanMismatch, firstBadCell,
                    firstBadCell < cellCount ? gpuCounts[firstBadCell] : 0,
                    firstBadCell < cellCount ? cpuCounts[firstBadCell] : 0,
                    firstBadCell < cellCount ? gpuOffsets[firstBadCell] : 0,
                    firstBadCell < cellCount ? cpuScan[firstBadCell] : 0);
        throw std::runtime_error("BM_GpuCellList: scan/count parity FAILED");
    }

    // 2) per-cell set equality (порядок игнорируем).
    uint32_t setMismatch = 0;
    for (uint32_t c = 0; c < cellCount; ++c) {
        const uint32_t begin = gpuOffsets[c];
        const uint32_t cnt = gpuCounts[c];
        if (cnt == 0) {
            continue;
        }
        if (static_cast<size_t>(begin) + cnt > gpuAtoms.size()) {
            std::printf("[FAIL] %s: cell %u slice [%u,%u) out of atomsInCells (size %zu)\n", s.name, c, begin, begin + cnt,
                        gpuAtoms.size());
            throw std::runtime_error("BM_GpuCellList: GPU cell slice out of range");
        }
        std::unordered_set<uint32_t> gpuSet(gpuAtoms.begin() + begin, gpuAtoms.begin() + begin + cnt);
        if (gpuSet.size() != cnt) {
            std::printf("[FAIL] %s: cell %u has duplicate atom indices in GPU slice (cnt=%u uniq=%zu)\n", s.name, c, cnt,
                        gpuSet.size());
            throw std::runtime_error("BM_GpuCellList: duplicate atom in GPU cell");
        }
        const std::span<const uint32_t> cpuCell = grid.atomsInCell(c);
        bool ok = (cpuCell.size() == gpuSet.size());
        if (ok) {
            for (uint32_t a : cpuCell) {
                if (gpuSet.find(a) == gpuSet.end()) {
                    ok = false;
                    break;
                }
            }
        }
        if (!ok) {
            if (setMismatch == 0) {
                std::printf("[FAIL] %s: cell %u atom SET differs (cpu n=%zu gpu n=%u)\n", s.name, c, cpuCell.size(), cnt);
            }
            ++setMismatch;
        }
    }
    if (setMismatch != 0) {
        std::printf("[FAIL] %s: per-cell set mismatch in %u cells\n", s.name, setMismatch);
        throw std::runtime_error("BM_GpuCellList: per-cell atom SET parity FAILED");
    }

    std::printf("[ CELLLIST] %-14s atoms=%-6u cells=%-7u maxCellCnt=%u  PARITY OK\n", s.name, atomCount, cellCount,
                *std::max_element(cpuCounts.begin(), cpuCounts.end()));
    std::fflush(stdout);
}

void runCellListParity(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        GpuNeighborListBuilder builder;
        // Несколько сцен через ОДИН builder: заодно проверяет рост буферов
        // (ensureCapacity) между разными размерами.
        checkScene(builder, makeSmallGrid());
        checkScene(builder, makeBoundary());
        checkScene(builder, makeDense());
        checkScene(builder, makeRandom(2000, 1));
        checkScene(builder, makeRandom(20000, 7));
        checkScene(builder, makeLargeRandom());
        checkScene(builder, makeHugeGrid()); // 3-уровневый scan (главный риск)
        // Прогон убывающего размера — буфера не должны переиспользоваться неверно.
        checkScene(builder, makeSmallGrid());
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuCellList/Parity","ru":"GPU cell-list parity vs CPU SpatialGrid","group":"Симуляция/GPU"}
void BM_GpuCellList_Parity(benchmark::State& state) { runCellListParity(state); }

BENCHMARK(BM_GpuCellList_Parity)->Iterations(1)->Unit(benchmark::kMicrosecond);
