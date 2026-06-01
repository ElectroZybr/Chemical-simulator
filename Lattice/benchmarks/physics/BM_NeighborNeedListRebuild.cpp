#include <benchmark/benchmark.h>
#include "Fixture.h"
using namespace Lattice;

// @bench_meta {"id":"Fixture/NeighborListNeedRebuild","label":"Check NLneedsRebuild","group":"Simulation/Grid and Neighbors"}
BENCHMARK_DEFINE_F(Fixture, NeighborListNeedRebuild)(benchmark::State& state) {
    rebuildScene();

    for (auto _ : state) {
        simulation_->neighborList().needsRebuild(simulation_->atoms());
        benchmark::DoNotOptimize(simulation_->neighborList().pairStorageSize());
        benchmark::ClobberMemory();
    }

    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, NeighborListNeedRebuild)
    ->Arg(5)
    ->Arg(10);
