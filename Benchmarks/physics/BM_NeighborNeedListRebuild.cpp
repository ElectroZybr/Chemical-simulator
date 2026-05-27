#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/SimulationFixture.h"

// @bench_meta {"id":"SimulationFixture/NeighborListNeedRebuild","ru":"Проверка NeighborList::needsRebuild","group":"Симуляция/Сетка и
// соседи"}
BENCHMARK_DEFINE_F(SimulationFixture, NeighborListNeedRebuild)(benchmark::State& state) {
    rebuildScene();
    // Исправление бага: этот benchmark раньше измерял invalid fast path. Сначала
    // строим valid list, чтобы timed loop измерял scan displacement.
    prepareNeighborList();

    for (auto _ : state) {
        const bool needsRebuild = simulation_->neighborList().needsRebuild(simulation_->atoms());
        benchmark::DoNotOptimize(needsRebuild);
        benchmark::ClobberMemory();
    }

    setCounters(state);
}

BENCHMARK_REGISTER_F(SimulationFixture, NeighborListNeedRebuild)->RangeMultiplier(8)->Range(Benchmarks::kAtomMin, Benchmarks::kAtomMax);
