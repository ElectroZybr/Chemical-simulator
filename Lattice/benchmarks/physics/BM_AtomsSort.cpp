#include "benchmark/benchmark.h"
#include "Fixture.h"

// @bench_meta {"id":"Fixture/AtomsSort","label":"AtomsSort","group":"Simulation/Forces"}
BENCHMARK_DEFINE_F(Fixture, AtomsSort)(benchmark::State& state) {
    rebuildScene();
    warmupScene();
    prepareNeighborList();
    StepData stepData = makeStepData();

    for (auto _ : state) {
        simulation_->atoms().sort(simulation_->world().getGrid());
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, AtomsSort)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Arg(47);