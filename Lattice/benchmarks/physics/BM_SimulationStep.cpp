#include <benchmark/benchmark.h>
#include "Fixture.h"
using namespace Lattice;

// @bench_meta {"id":"Fixture/FullStep","label":"Full Step","group":"Simulation/Simulation Step"}
BENCHMARK_DEFINE_F(Fixture, FullStepWithNeighborList)(benchmark::State& state) {
    rebuildScene();
    warmupScene();
    const size_t rebuildCountBefore = simulation_->neighborList().stats().rebuildCount();

    for (auto _ : state) {
        simulation_->update();
        benchmark::ClobberMemory();
    }

    const size_t rebuildCountAfter = simulation_->neighborList().stats().rebuildCount();
    const size_t rebuildCount = rebuildCountAfter - rebuildCountBefore;
    const double iterCount = static_cast<double>(state.iterations());

    state.counters["nl_rebuild_count"] = static_cast<double>(rebuildCount);
    state.counters["nl_rebuilds_per_step"] = (iterCount > 0.0) ? static_cast<double>(rebuildCount) / iterCount : 0.0;
    state.counters["nl_avg_steps_between_rebuilds"] =
        static_cast<double>(simulation_->neighborList().stats().averageStepsBetweenRebuilds());
    state.counters["nl_steps_since_last_rebuild"] =
        static_cast<double>(simulation_->neighborList().stats().stepsSinceLastRebuild(simulation_->getSimStep()));

    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, FullStepWithNeighborList)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Arg(47)
    ->Arg(100);
