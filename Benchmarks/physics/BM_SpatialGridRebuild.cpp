#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/SimulationFixture.h"

// @bench_meta {"id":"SimulationFixture/SpatialGridRebuild","ru":"Перестройка SpatialGrid","group":"Симуляция/Сетка и соседи"}
BENCHMARK_DEFINE_F(SimulationFixture, SpatialGridRebuild)(benchmark::State& state) {
    rebuildScene();

    auto& atoms = simulation_->atoms();
    auto& grid = simulation_->world().getGrid();

    for (auto _ : state) {
        grid.rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());
        benchmark::DoNotOptimize(grid.stats().lastNonEmptyCellCount());
        benchmark::ClobberMemory();
    }

    setCounters(state);
}

BENCHMARK_REGISTER_F(SimulationFixture, SpatialGridRebuild)->RangeMultiplier(8)->Range(Benchmarks::kAtomMin, Benchmarks::kAtomMax);
