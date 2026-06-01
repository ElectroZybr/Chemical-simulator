#include <benchmark/benchmark.h>
#include <vector>
#include "Fixture.h"
using namespace Lattice;

namespace {
    struct VelocityPostProcessData {
        std::vector<float> baseVx;
        std::vector<float> baseVy;
        std::vector<float> baseVz;
    };

    VelocityPostProcessData prepareVelocityPostProcessData(Lattice::Simulation& simulation) {
        VelocityPostProcessData data;
        const AtomStorage& atomStorage = simulation.atoms();
        const std::size_t mobileCount = atomStorage.mobileCount();

        data.baseVx.assign(atomStorage.vxData(), atomStorage.vxData() + mobileCount);
        data.baseVy.assign(atomStorage.vyData(), atomStorage.vyData() + mobileCount);
        data.baseVz.assign(atomStorage.vzData(), atomStorage.vzData() + mobileCount);
        return data;
    }

    void restoreVelocities(AtomStorage& atomStorage, const VelocityPostProcessData& data) {
        std::copy(data.baseVx.begin(), data.baseVx.end(), atomStorage.vxData());
        std::copy(data.baseVy.begin(), data.baseVy.end(), atomStorage.vyData());
        std::copy(data.baseVz.begin(), data.baseVz.end(), atomStorage.vzData());
    }
}

// @bench_meta {"id":"Fixture/PostProcessVelocities","label":"Velocity Post-Process: clamp","group":"Simulation/Integrator"}
BENCHMARK_DEFINE_F(Fixture, PostProcessVelocities)(benchmark::State& state) {
    rebuildScene();
    VelocityPostProcessData data = prepareVelocityPostProcessData(*simulation_);

    for (auto _ : state) {
        state.PauseTiming();
        restoreVelocities(simulation_->atoms(), data);
        state.ResumeTiming();

        StepOps::postProcessVelocities(simulation_->atoms(), 1.0f);

        benchmark::DoNotOptimize(simulation_->atoms().vxData()[0]);
        benchmark::ClobberMemory();
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, PostProcessVelocities)
    ->Arg(5)
    ->Arg(10);
