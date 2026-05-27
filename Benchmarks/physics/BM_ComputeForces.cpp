#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/SimulationFixture.h"

// @bench_meta {"id":"SimulationFixture/ComputeForcesWithNeighborList","ru":"Расчет сил с NeighborList","group":"Симуляция/Силы"}
BENCHMARK_DEFINE_F(SimulationFixture, ComputeForcesWithNeighborList)(benchmark::State& state) {
    rebuildScene();
    prepareNeighborList();
    StepData stepData = makeStepData();

    for (auto _ : state) {
        StepOps::computeForces(stepData);
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

// @bench_meta {"id":"SimulationFixture/ComputePairInteractionsWithNeighborList","ru":"Расчет PairInteraction взаимодействий с NeighborList","group":"Симуляция/Силы"}
BENCHMARK_DEFINE_F(SimulationFixture, ComputePairInteractionsWithNeighborList)(benchmark::State& state) {
    rebuildScene();
    prepareNeighborList();

    for (auto _ : state) {
        simulation_->forceField().computePairInteractions(simulation_->world());
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(SimulationFixture, ComputeForcesWithNeighborList)
    ->RangeMultiplier(8)
    ->Range(Benchmarks::kAtomMin, Benchmarks::kAtomMax)
    ->Args({15625})   // 25^3
    ->Args({103823}); // 47^3

BENCHMARK_REGISTER_F(SimulationFixture, ComputePairInteractionsWithNeighborList)
    ->RangeMultiplier(8)
    ->Range(Benchmarks::kAtomMin, Benchmarks::kAtomMax)
    ->Args({15625})   // 25^3
    ->Args({103823}); // 47^3
