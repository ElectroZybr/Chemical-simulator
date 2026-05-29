// Гейт свежести диагностического CPU SpatialGrid в GPU-режиме (задача 2f).
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Падение/паника абортит прогон.
//
// КОНТЕКСТ. После 2d hot loop в GPU-режиме перестраивает NeighborList ЦЕЛИКОМ на
// GPU и CPU SpatialGrid НЕ трогает. Грид застывает на снимке времени входа в
// GPU-режим, а атомы в VRAM уезжают — поэтому CPU-грид-потребители (визуализация
// сетки drawGrid, overlay соседей, debug-панель) показывают устаревшие клетки
// («сетка отделяется от частиц»). Фикс 2f: App вызывает refreshDiagnosticsGrid()
// на каденции РЕНДЕРА (после syncFromGpuIfNeeded), перебиннивая грид из свежих
// CPU-позиций — это O(N) раз в кадр, а не CPU round-trip в hot loop, что убрала 2d.
//
// ГЕЙТ доказывает ДВА факта:
//   (1) СВЕЖЕСТЬ: после step + sync + refreshDiagnosticsGrid биннинг CPU-грида
//       совпадает с НЕЗАВИСИМЫМ пере-биннингом синканных позиций (та же
//       координатная трансформация грида, но эталонный CSR строится в гейте с
//       нуля). Сверяем множество непустых клеток, почленное содержимое клеток и
//       linearCellOfAtom каждого атома.
//   (2) НЕ-ВАКУАТИВНОСТЬ: ДО refresh (грид застыл на входном снимке) тот же диф
//       НАХОДИТ расхождение — значит гейт реально отличает свежий грид от
//       устаревшего, а не проходит тривиально. Если бы grid и так был свежим,
//       этот гейт не имел бы смысла.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <vector>

#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/Simulation.h"
#include "Engine/World.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"

namespace {

// Эталонный биннинг: cell -> отсортированный список атомов, посчитанный НЕЗАВИСИМО
// из переданных позиций через координатную трансформацию грида (worldToCell*,
// index — чистые функции позиции, НЕ читают хранимый CSR). Это и есть независимая
// ссылка, с которой сверяется хранимое в гриде состояние.
std::map<int, std::vector<uint32_t>> referenceBinning(const SpatialGrid& grid, const AtomStorage& a) {
    std::map<int, std::vector<uint32_t>> bins;
    for (size_t i = 0; i < a.size(); ++i) {
        const int cell = grid.index(grid.worldToCellX(a.posX(i)), grid.worldToCellY(a.posY(i)), grid.worldToCellZ(a.posZ(i)));
        bins[cell].push_back(static_cast<uint32_t>(i));
    }
    return bins;
}

// Хранимый в гриде биннинг: cell -> отсортированный список атомов, прочитанный из
// CSR (nonEmptyCells + atomsInCell). Это то, что реально читают viz/overlay/stats.
std::map<int, std::vector<uint32_t>> storedBinning(const SpatialGrid& grid) {
    std::map<int, std::vector<uint32_t>> bins;
    for (uint32_t cell : grid.nonEmptyCells()) {
        std::vector<uint32_t> members;
        for (uint32_t idx : grid.atomsInCell(static_cast<size_t>(cell))) {
            members.push_back(idx);
        }
        std::sort(members.begin(), members.end());
        bins[static_cast<int>(cell)] = std::move(members);
    }
    return bins;
}

// Сравнивает хранимый CSR грида с эталоном из позиций. Возвращает число
// расхождений (0 == грид свеж относительно позиций). Сверяет: множество непустых
// клеток, почленное содержимое каждой клетки и linearCellOfAtom каждого атома.
size_t countGridMismatches(const SpatialGrid& grid, const AtomStorage& a) {
    std::map<int, std::vector<uint32_t>> reference = referenceBinning(grid, a);
    for (auto& [cell, members] : reference) {
        std::sort(members.begin(), members.end());
    }
    const std::map<int, std::vector<uint32_t>> stored = storedBinning(grid);

    size_t mismatches = 0;
    if (reference != stored) {
        // Считаем клетки, различающиеся составом/наличием (диагностика + метрика).
        std::map<int, std::vector<uint32_t>> all = reference;
        for (const auto& [cell, members] : stored) {
            all.try_emplace(cell);
        }
        for (const auto& [cell, _] : all) {
            const auto rIt = reference.find(cell);
            const auto sIt = stored.find(cell);
            const bool rHas = rIt != reference.end();
            const bool sHas = sIt != stored.end();
            if (rHas != sHas || (rHas && sHas && rIt->second != sIt->second)) {
                ++mismatches;
            }
        }
    }

    // Дополнительно per-atom: linearCellOfAtom обязан совпасть с пересчитанной из
    // позиции клеткой (ловит рассинхрон cellIndices_ даже при совпавшем CSR).
    for (size_t i = 0; i < a.size(); ++i) {
        const int expected = grid.index(grid.worldToCellX(a.posX(i)), grid.worldToCellY(a.posY(i)), grid.worldToCellZ(a.posZ(i)));
        if (grid.linearCellOfAtom(static_cast<uint32_t>(i)) != expected) {
            ++mismatches;
        }
    }
    return mismatches;
}

void runGpuDiagnosticsGrid(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld(Vec3f{160.0f, 160.0f, 160.0f});
        sim.setSizeBox(Vec3f{160.0f, 160.0f, 160.0f}, 6);
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
        sim.setDt(0.01f);

        // 8^3 = 512 атомов с большими скоростями: смещение v*t = ~8*0.6 ~= нескольких
        // cellSize (6) за 60 шагов, так что многие атомы СМЕНЯТ клетку и застывший
        // на входном снимке грид ГАРАНТИРОВАННО разойдётся с текущими позициями.
        const int side = 8;
        for (int z = 0; z < side; ++z) {
            for (int y = 0; y < side; ++y) {
                for (int x = 0; x < side; ++x) {
                    // Детерминированные скорости (без RNG): разнонаправленный разлёт.
                    const float vx = 8.0f * static_cast<float>((x % 3) - 1);
                    const float vy = 8.0f * static_cast<float>((y % 3) - 1);
                    const float vz = 8.0f * static_cast<float>((z % 3) - 1);
                    sim.appendAtomFast(Vec3f{50.0f + x * 3.0f, 50.0f + y * 3.0f, 50.0f + z * 3.0f}, Vec3f{vx, vy, vz},
                                       AtomData::Type::H);
                }
            }
        }
        sim.finalizeAtomBatch(); // строит CPU-грид из исходных позиций (входной снимок)

        // Снимок исходных позиций — для замера фактического смещения (диагностика
        // не-вакуативности: атомы обязаны реально уехать на >= cellSize).
        std::vector<float> startX(sim.atoms().size()), startY(sim.atoms().size()), startZ(sim.atoms().size());
        for (size_t i = 0; i < sim.atoms().size(); ++i) {
            startX[i] = sim.atoms().posX(i);
            startY[i] = sim.atoms().posY(i);
            startZ[i] = sim.atoms().posZ(i);
        }

        sim.setGpuMode(true);
        for (int s = 0; s < 60; ++s) {
            sim.update(); // GPU шагает; CPU-грид НЕ трогается (после 2d)
        }
        sim.syncFromGpuIfNeeded(); // CPU-позиции теперь свежие (атомы уехали)

        const World& world = sim.world();
        const AtomStorage& atoms = sim.atoms();

        double maxDisp = 0.0;
        for (size_t i = 0; i < atoms.size(); ++i) {
            const double dx = atoms.posX(i) - startX[i], dy = atoms.posY(i) - startY[i], dz = atoms.posZ(i) - startZ[i];
            maxDisp = std::max(maxDisp, std::sqrt(dx * dx + dy * dy + dz * dz));
        }

        // --- (2) НЕ-ВАКУАТИВНОСТЬ: ДО refresh грид застыл => обязан разойтись. ---
        const size_t staleMismatches = countGridMismatches(world.getGrid(), atoms);

        // --- (1) СВЕЖЕСТЬ: refresh перебиннивает грид из свежих позиций. ---
        sim.refreshDiagnosticsGrid();
        const size_t freshMismatches = countGridMismatches(world.getGrid(), atoms);

        std::printf("[ DIAG-GRID] atoms=%zu maxDisp=%.3f (cellSize=%.1f) stale_mismatches(before refresh)=%zu fresh_mismatches(after)=%zu\n",
                    atoms.size(), maxDisp, world.getGrid().cellSize, staleMismatches, freshMismatches);
        std::fflush(stdout);
        state.counters["max_disp"] = maxDisp;
        state.counters["stale_mismatches"] = static_cast<double>(staleMismatches);
        state.counters["fresh_mismatches"] = static_cast<double>(freshMismatches);

        // Свежесть: после refresh грид ОБЯЗАН точно совпасть с эталоном из позиций.
        if (freshMismatches != 0) {
            throw std::runtime_error("refreshDiagnosticsGrid: CPU-грид НЕ совпал с пере-биннингом синканных позиций (грид не свеж)");
        }
        // Не-вакуативность: без refresh грид был устаревшим — иначе гейт не доказывает,
        // что refresh что-то чинит. Если staleMismatches==0, сцена двигалась слишком
        // мало (клетки не пересекались) — гейт стал бы тривиальным.
        if (staleMismatches == 0) {
            throw std::runtime_error("гейт вакуативен: грид был свеж ДО refresh (атомы не сменили клетки — усилить движение)");
        }

        sim.setGpuMode(false); // обратный toggle должен оставаться рабочим
    }
    state.SetItemsProcessed(state.iterations());
}

// Мульти-мир: рендер рисует ВСЕ миры, поэтому refreshDiagnosticsGrid обязан
// перебиннить грид и НЕактивного GPU-мира. Раньше рефрешился только активный —
// неактивный GPU-мир показывал бы замороженную сетку при drawGrid. Здесь world 0 —
// GPU и НЕактивен (активен world 1), updateAll шагает оба; проверяем грид world 0.
void runGpuDiagnosticsGridMultiWorld(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld(Vec3f{160.0f, 160.0f, 160.0f}); // world 0
        sim.setSizeBox(Vec3f{160.0f, 160.0f, 160.0f}, 6);
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
        sim.setDt(0.01f);

        const int side = 8; // 512 движущихся атомов (как single-world кейс)
        for (int z = 0; z < side; ++z) {
            for (int y = 0; y < side; ++y) {
                for (int x = 0; x < side; ++x) {
                    const float vx = 8.0f * static_cast<float>((x % 3) - 1);
                    const float vy = 8.0f * static_cast<float>((y % 3) - 1);
                    const float vz = 8.0f * static_cast<float>((z % 3) - 1);
                    sim.appendAtomFast(Vec3f{50.0f + x * 3.0f, 50.0f + y * 3.0f, 50.0f + z * 3.0f}, Vec3f{vx, vy, vz},
                                       AtomData::Type::H);
                }
            }
        }
        sim.finalizeAtomBatch();
        sim.setGpuMode(true); // world 0 -> GPU (пока активен)

        sim.createWorld(Vec3f{160.0f, 160.0f, 160.0f}); // world 1
        sim.setActiveWorld(1);                          // world 0 теперь GPU + НЕактивен

        for (int s = 0; s < 60; ++s) {
            sim.updateAll(); // шагает ОБА мира; world 0 (GPU) уезжает в VRAM
        }
        sim.syncFromGpuIfNeeded(); // синкает все GPU-миры -> CPU-позиции world 0 свежие

        // НЕ-ВАКУАТИВНОСТЬ: грид НЕактивного world 0 ДО refresh застыл на входном
        // снимке => обязан разойтись с уехавшими позициями.
        sim.setActiveWorld(0);
        const size_t staleMismatches = countGridMismatches(sim.world().getGrid(), sim.atoms());

        // refresh вызываем пока world 0 НЕактивен — именно это и было непокрыто.
        sim.setActiveWorld(1);
        sim.refreshDiagnosticsGrid();

        // СВЕЖЕСТЬ: грид world 0 совпал с пере-биннингом его синканных позиций.
        sim.setActiveWorld(0);
        const size_t freshMismatches = countGridMismatches(sim.world().getGrid(), sim.atoms());

        std::printf("[ DIAG-GRID-MW] world0 (inactive GPU): stale(before)=%zu fresh(after)=%zu\n", staleMismatches, freshMismatches);
        std::fflush(stdout);
        state.counters["stale_mismatches"] = static_cast<double>(staleMismatches);
        state.counters["fresh_mismatches"] = static_cast<double>(freshMismatches);

        if (freshMismatches != 0) {
            throw std::runtime_error("refreshDiagnosticsGrid: грид НЕактивного GPU-мира не перебиннен (мульти-мир не покрыт)");
        }
        if (staleMismatches == 0) {
            throw std::runtime_error("гейт вакуативен: грид неактивного world 0 был свеж ДО refresh (усилить движение)");
        }

        sim.setGpuMode(false); // обратный toggle активного world 0 должен работать
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuDiagnosticsGrid/FreshAfterRefresh","ru":"GPU: диагностический грид свеж после refresh","group":"Симуляция/GPU"}
void BM_GpuDiagnosticsGrid_FreshAfterRefresh(benchmark::State& state) { runGpuDiagnosticsGrid(state); }

BENCHMARK(BM_GpuDiagnosticsGrid_FreshAfterRefresh)->Iterations(1)->Unit(benchmark::kMicrosecond);

// @bench_meta {"id":"GpuDiagnosticsGrid/MultiWorldFresh","ru":"GPU: грид неактивного мира свеж после refresh","group":"Симуляция/GPU"}
void BM_GpuDiagnosticsGrid_MultiWorldFresh(benchmark::State& state) { runGpuDiagnosticsGridMultiWorld(state); }

BENCHMARK(BM_GpuDiagnosticsGrid_MultiWorldFresh)->Iterations(1)->Unit(benchmark::kMicrosecond);
