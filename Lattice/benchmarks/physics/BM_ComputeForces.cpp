#include <benchmark/benchmark.h>
#include "Fixture.h"
using namespace Lattice;

// @bench_meta {"id":"Fixture/ComputeForces","label":"Force Computation","group":"Simulation/Forces"}
BENCHMARK_DEFINE_F(Fixture, ComputeForces)(benchmark::State& state) {
    rebuildScene();
    warmupScene();
    prepareNeighborList();
    StepData stepData = makeStepData();

    for (auto _ : state) {
        StepOps::computeForces(stepData);
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, ComputeForces)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Arg(47);


// @bench_meta {"id":"Fixture/ComputePairInteractions","label":"PairInteraction Computation","group":"Simulation/Forces"}
BENCHMARK_DEFINE_F(Fixture, ComputePairInteractions)(benchmark::State& state) {
    rebuildScene();
    warmupScene();
    prepareNeighborList();

    for (auto _ : state) {
        simulation_->forceField().computePairInteractions(simulation_->world());
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, ComputePairInteractions)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Arg(47);
