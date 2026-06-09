#include <benchmark/benchmark.h>
#include "Fixture.h"
using namespace Lattice;

// @bench_meta {"id":"Fixture/NeighborListRebuild","label":"NL Rebuild","group":"Simulation/Grid and Neighbors"}
BENCHMARK_DEFINE_F(Fixture, NeighborListRebuildOnly)(benchmark::State& state) {
    rebuildScene();
    warmupScene();

    auto& atoms = simulation_->atoms();
    auto& grid = simulation_->world().getGrid();
    grid.rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());

    for (auto _ : state) {
        simulation_->neighborList().build(simulation_->atoms(), simulation_->world());
        benchmark::DoNotOptimize(simulation_->neighborList().pairStorageSize());
        benchmark::ClobberMemory();
    }

    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, NeighborListRebuildOnly)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Arg(47);
