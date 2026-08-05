#include <benchmark/benchmark.h>

#include "Fixture.h"
#include "Lattice/Plugins/ClassicMDMT/Integrators/StepOps.h"
#include "Lattice/Plugins/ClassicMDMT/Integrators/Verlet.h"

using Fixture = Benchmarks::Fixture;

// @bench_meta {"id":"Fixture/CorrectMT","label":"Correct MT","group":"Simulation/Integrator"}
BENCHMARK_DEFINE_F(Fixture, CorrectMT)(benchmark::State& state) {
    prepareForCorrect();

    for (auto _ : state) {
        VerletMT::correct(simulation_->atoms(), Benchmarks::kDt);
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
