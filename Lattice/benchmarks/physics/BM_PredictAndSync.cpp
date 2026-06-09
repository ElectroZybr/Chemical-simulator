#include <benchmark/benchmark.h>
#include "Fixture.h"
using namespace Lattice;

// @bench_meta {"id":"Fixture/PredictAndSync","label":"Predict + Sync","group":"Simulation/Integrator"}
BENCHMARK_DEFINE_F(Fixture, PredictAndSync)(benchmark::State& state) {
    prepareForPredict();
    StepData stepData = makeStepData();

    for (auto _ : state) {
        StepData stepData = makeStepData();
        StepOps::predictAndSync(stepData, &VerletScheme::predict);
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, PredictAndSync)
    ->Arg(5)
    ->Arg(10);
