#include <benchmark/benchmark.h>
#include "Fixture.h"

// @bench_meta {"id":"Fixture/CorrectMT","label":"Correct MT","group":"Simulation/Integrator"}
BENCHMARK_DEFINE_F(Fixture, CorrectMT)(benchmark::State& state) {
    prepareForCorrect();

    for (auto _ : state) {
        Verlet::correct(simulation_->atoms(), Benchmarks::kDt);
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, CorrectMT)
    ->Arg(5)
    ->Arg(10)
    ->Arg(22)
    ->Arg(47);
