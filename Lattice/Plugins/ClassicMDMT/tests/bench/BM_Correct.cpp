#include <benchmark/benchmark.h>

#include "Lattice/Plugins/ClassicMDMT/Integrators/StepOps.h"
#include "Lattice/Plugins/ClassicMDMT/Integrators/Verlet.h"

// @bench_meta {"id":"Fixture/CorrectMT","label":"Correct MT","group":"Simulation/Integrator"}
BENCHMARK_DEFINE_F(FixtureMT, CorrectMT)(benchmark::State& state) {
    prepareForCorrect();

    for (auto _ : state) {
        VerletMT::correct(simulation_->atoms(), Benchmarks::kDt);
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(FixtureMT, CorrectMT)
    ->Arg(5)
    ->Arg(10)
    ->Arg(22)
    ->Arg(47);
