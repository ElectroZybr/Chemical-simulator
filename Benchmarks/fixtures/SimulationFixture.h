#pragma once

#include <cstdlib>
#include <memory>
#include <string>

#include <benchmark/benchmark.h>

#include "Benchmarks/BenchmarkCase.h"
#include "Benchmarks/BenchmarkScenes.h"
#include "Engine/Simulation.h"
#include "Engine/physics/integrators/StepOps.h"
#include "Engine/physics/integrators/VerletScheme.h"

namespace Benchmarks {
    constexpr double kDt = 0.01;
    constexpr int kAtomMin = 125;  // 5^3
    constexpr int kAtomMax = 1000; // 10^3

    inline SceneKind sceneFromEnv() {
        const char* raw = std::getenv("CHEM_BENCH_SCENE");
        if (raw == nullptr) {
            return SceneKind::Crystal3D;
        }
        const std::string value(raw);
        if (value == "ideal_crystal3d") {
            return SceneKind::IdealCrystal3D;
        }
        if (value == "crystal2d") {
            return SceneKind::Crystal2D;
        }
        if (value == "random_gas2d") {
            return SceneKind::RandomGas2D;
        }
        return SceneKind::Crystal3D;
    }

    inline BenchmarkCase makeCaseForSelectedScene(int atomCount) {
        BenchmarkCase benchmarkCase{.scene = sceneFromEnv(),
                                    .integrator = Integrator::Scheme::Verlet,
                                    .atomCount = atomCount,
                                    .boxSize = Vec3f(160.0, 160.0, 160.0),
                                    .cellSize = 5};

        if (benchmarkCase.scene == SceneKind::Crystal2D || benchmarkCase.scene == SceneKind::RandomGas2D) {
            benchmarkCase.boxSize = Vec3f(160.0, 160.0, 6.0);
        }
        return benchmarkCase;
    }
}

class SimulationFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        atomCount_ = static_cast<int>(state.range(0));
        simulation_ = std::make_unique<Simulation>();
        simulation_->createWorld(Vec3f(160, 160, 160));
    }

    void TearDown(benchmark::State&) override { simulation_.reset(); }

protected:
    StepData makeStepData(float accelDamping = 0.9f) {
        return StepData{
            .world = simulation_->world(),
            .forceField = simulation_->forceField(),
            .neighborList = simulation_->neighborList(),
            .allowBondFormation = simulation_->isBondFormationEnabled(),
            .accelDamping = accelDamping,
            .dt = static_cast<float>(Benchmarks::kDt),
        };
    }

    void rebuildScene() { Benchmarks::BenchmarkScenes::build(*simulation_, Benchmarks::makeCaseForSelectedScene(atomCount_)); }

    void prepareForPredict() {
        rebuildScene();
        prepareNeighborList();
        StepData stepData = makeStepData();
        StepOps::computeForces(stepData);
    }

    void prepareNeighborList() { simulation_->neighborList().build(simulation_->atoms(), simulation_->world()); }

    void prepareForCorrect() {
        prepareForPredict();
        StepData stepData = makeStepData();
        StepOps::predictAndSync(stepData, &VerletScheme::predict);
        StepOps::computeForces(stepData);
    }

    void setCounters(benchmark::State& state) const {
        const int64_t processedAtoms = simulation_ ? static_cast<int64_t>(simulation_->atoms().size()) : static_cast<int64_t>(atomCount_);
        state.SetItemsProcessed(state.iterations() * processedAtoms);
    }

    std::unique_ptr<Simulation> simulation_;
    int atomCount_ = 0;
};
