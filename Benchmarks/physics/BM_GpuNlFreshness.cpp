// Гейт свежести NL в GPU-режиме под БЫСТРЫМ движением.
//
// В GPU-режиме решение о перестройке NL батчится: смещение проверяется раз в
// kDispCheckCadence шагов (updateStateGpu), а не каждый шаг как на CPU. Если за
// эти шаги атом уезжает дальше skin от reference-позиции, NL устаревает и теряет
// пары, попавшие в cutoff — силы считаются неверно (видимый симптом: «сетка
// отделяется от частиц»).
//
// Тест прямой: прогоняем горячую сцену в GPU-режиме, затем сверяем — все пары в
// пределах ФИЗИЧЕСКОГО cutoff на ТЕКУЩИХ позициях обязаны присутствовать в NL.
// Брутфорс O(N^2) даёт эталон. Любая потерянная пара = устаревший NL.
//
// BM_GpuCorrectness намеренно использует медленную сцену без перестроек, поэтому
// этот класс багов не покрывает — данный гейт закрывает пробел.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"

namespace {

using PairSet = std::set<std::pair<uint32_t, uint32_t>>;

PairSet nlPairsWithinCutoff(const Simulation& sim) {
    const NeighborList& nl = sim.neighborList();
    const AtomStorage& a = sim.atoms();
    const auto& offsets = nl.offsets();
    const auto& neighbors = nl.neighbors();
    const float cutoffSqr = nl.cutoff() * nl.cutoff();
    const uint32_t mobile = static_cast<uint32_t>(a.mobileCount());
    PairSet pairs;
    for (uint32_t i = 0; i < mobile; ++i) {
        for (uint32_t p = offsets[i]; p < offsets[i + 1]; ++p) {
            const uint32_t j = neighbors[p];
            if (j >= mobile) {
                continue;
            }
            const float dx = a.posX(j) - a.posX(i), dy = a.posY(j) - a.posY(i), dz = a.posZ(j) - a.posZ(i);
            if (dx * dx + dy * dy + dz * dz <= cutoffSqr) {
                pairs.emplace(std::min(i, j), std::max(i, j));
            }
        }
    }
    return pairs;
}

PairSet bruteForceWithinCutoff(const Simulation& sim) {
    const AtomStorage& a = sim.atoms();
    const float cutoffSqr = sim.neighborList().cutoff() * sim.neighborList().cutoff();
    const uint32_t mobile = static_cast<uint32_t>(a.mobileCount());
    PairSet pairs;
    for (uint32_t i = 0; i < mobile; ++i) {
        for (uint32_t j = i + 1; j < mobile; ++j) {
            const float dx = a.posX(j) - a.posX(i), dy = a.posY(j) - a.posY(i), dz = a.posZ(j) - a.posZ(i);
            if (dx * dx + dy * dy + dz * dz <= cutoffSqr) {
                pairs.emplace(i, j);
            }
        }
    }
    return pairs;
}

void runGpuNlFreshness(benchmark::State& state) {
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

        // Горячая сцена: 512 атомов, шаг 3, но БОЛЬШИЕ случайные скорости —
        // атомы уезжают >0.125/шаг, т.е. >0.5 за 4 шага (кадэнс проверки), что
        // способно перебросить смещение за skin между проверками.
        std::mt19937 rng(2024);
        std::uniform_real_distribution<float> vel(-2.0f, 2.0f);
        const int side = 8; // 8^3 = 512
        for (int z = 0; z < side; ++z) {
            for (int y = 0; y < side; ++y) {
                for (int x = 0; x < side; ++x) {
                    sim.appendAtomFast(Vec3f{50.0f + x * 3.0f, 50.0f + y * 3.0f, 50.0f + z * 3.0f},
                                       Vec3f{vel(rng), vel(rng), vel(rng)}, AtomData::Type::H);
                }
            }
        }
        sim.finalizeAtomBatch();

        sim.setGpuMode(true);
        for (int s = 0; s < 40; ++s) {
            sim.update();
        }
        sim.syncFromGpuIfNeeded(); // текущие позиции в CPU

        const PairSet nlPairs = nlPairsWithinCutoff(sim);
        const PairSet bfPairs = bruteForceWithinCutoff(sim);

        // Пары, которые физически в cutoff, но отсутствуют в NL = устаревание.
        PairSet missing;
        std::set_difference(bfPairs.begin(), bfPairs.end(), nlPairs.begin(), nlPairs.end(),
                            std::inserter(missing, missing.begin()));
        std::printf("[ NL-FRESH ] within-cutoff pairs: bruteforce=%zu nl=%zu MISSING=%zu\n", bfPairs.size(), nlPairs.size(),
                    missing.size());
        state.counters["missing_pairs"] = static_cast<double>(missing.size());
        if (!missing.empty()) {
            throw std::runtime_error("GPU NL устарел: пары в cutoff потеряны (батчинг проверки смещения пропустил перестройку)");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuNlFreshness/NoLostPairsUnderFastMotion","ru":"GPU NL свеж под быстрым движением","group":"Симуляция/GPU"}
void BM_GpuNlFreshness_NoLostPairs(benchmark::State& state) { runGpuNlFreshness(state); }

BENCHMARK(BM_GpuNlFreshness_NoLostPairs)->Iterations(1)->Unit(benchmark::kMicrosecond);
