#include <benchmark/benchmark.h>
#include "Fixture.h"
using namespace Lattice;

// @bench_meta {"id":"Fixture/SpatialGridRebuild","label":"SpatialGrid Rebuild","group":"Simulation/Grid and Neighbors"}
BENCHMARK_DEFINE_F(Fixture, SpatialGridRebuild)(benchmark::State& state) {
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

BENCHMARK_REGISTER_F(Fixture, SpatialGridRebuild)
    ->Arg(5)
    ->Arg(10);
