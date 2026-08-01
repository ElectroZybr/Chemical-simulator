#include <benchmark/benchmark.h>
#include "Fixture.h"

using Fixture = Benchmarks::Fixture;

// @bench_meta {"id":"Fixture/Correct","label":"Correct","group":"Simulation/Integrator"}
BENCHMARK_DEFINE_F(Fixture, Correct)(benchmark::State& state) {
    prepareForCorrect();

    for (auto _ : state) {
        Verlet::correct(simulation_->atoms(), Benchmarks::kDt);
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, Correct)
    ->Arg(5)
    ->Arg(10)
    ->Arg(22)
    ->Arg(47);
