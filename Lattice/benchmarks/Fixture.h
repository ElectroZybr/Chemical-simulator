#pragma once

#include <memory>
#include <string_view>

#include <benchmark/benchmark.h>

#include "BenchmarkScenes.h"
#include "Engine/Simulation.h"
#include "Engine/physics/integrators/StepOps.h"
#include "Engine/physics/integrators/VerletScheme.h"

namespace Benchmarks {
    constexpr double kDt = 0.01;

    inline SceneKind sceneFromString(std::string_view value) {
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

    inline SceneKind& selectedScene() {
        static SceneKind scene = SceneKind::Crystal3D;
        return scene;
    }

    inline void setSelectedScene(SceneKind scene) { selectedScene() = scene; }

    inline SceneKind currentScene() { return selectedScene(); }

    inline int atomCountFromExtent(SceneKind scene, int sceneExtent) {
        switch (scene) {
        case SceneKind::IdealCrystal3D:
        case SceneKind::Crystal3D:
            return sceneExtent * sceneExtent * sceneExtent;
        case SceneKind::Crystal2D:
        case SceneKind::RandomGas2D:
            return sceneExtent * sceneExtent;
        }

        return sceneExtent;
    }
}

class Fixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        sceneExtent_ = static_cast<int>(state.range(0));
        atomCount_ = Benchmarks::atomCountFromExtent(Benchmarks::currentScene(), sceneExtent_);
        simulation_ = std::make_unique<Lattice::Simulation>();
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

    void rebuildScene() {
        Benchmarks::Scenes::build(*simulation_, Benchmarks::currentScene(), atomCount_);
    }

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

    std::unique_ptr<Lattice::Simulation> simulation_;
    int sceneExtent_ = 0;
    int atomCount_ = 0;
};
