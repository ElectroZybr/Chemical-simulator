#pragma once

#include <algorithm>
#include <memory>
#include <string_view>

#include <benchmark/benchmark.h>

#include "BenchmarkScenes.h"
#include "Engine/Simulation.h"
#include "Engine/physics/integrators/StepOps.h"
#include "Engine/physics/integrators/VerletScheme.h"

namespace Benchmarks {
    constexpr double kDt = 0.01;
    constexpr int kTemporalDegradationExtent = 25;

    enum class DegradationCriterion {
        Size,
        Time,
    };

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

    inline int& selectedWarmupSteps() {
        static int warmupSteps = 0;
        return warmupSteps;
    }

    inline void setSelectedWarmupSteps(int warmupSteps) { selectedWarmupSteps() = std::max(0, warmupSteps); }

    inline int currentWarmupSteps() { return selectedWarmupSteps(); }

    inline int temporalAgeStepsFromArg(int arg) {
        switch (arg) {
        case 5:
            return 0;
        case 10:
            return 100;
        case 22:
            return 500;
        case 25:
            return 1000;
        case 47:
            return 5000;
        default:
            return std::max(0, arg);
        }
    }

    inline DegradationCriterion degradationCriterionFromString(std::string_view value) {
        if (value == "time") {
            return DegradationCriterion::Time;
        }
        return DegradationCriterion::Size;
    }

    inline DegradationCriterion& selectedDegradationCriterion() {
        static DegradationCriterion criterion = DegradationCriterion::Size;
        return criterion;
    }

    inline void setSelectedDegradationCriterion(DegradationCriterion criterion) { selectedDegradationCriterion() = criterion; }

    inline DegradationCriterion currentDegradationCriterion() { return selectedDegradationCriterion(); }

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
        if (Benchmarks::currentDegradationCriterion() == Benchmarks::DegradationCriterion::Time) {
            sceneExtent_ = Benchmarks::kTemporalDegradationExtent;
            sceneAgeSteps_ = Benchmarks::temporalAgeStepsFromArg(static_cast<int>(state.range(0)));
        } else {
            sceneExtent_ = static_cast<int>(state.range(0));
            sceneAgeSteps_ = 0;
        }
        atomCount_ = Benchmarks::atomCountFromExtent(Benchmarks::currentScene(), sceneExtent_);
        simulation_ = std::make_unique<Lattice::Simulation>();
        simulation_->createWorld(glm::vec3(160.0f));
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
        ageScene();
    }

    void ageScene() {
        if (sceneAgeSteps_ <= 0) {
            return;
        }

        simulation_->setDt(Benchmarks::kDt);
        for (int i = 0; i < sceneAgeSteps_; ++i) {
            simulation_->update();
        }
    }

    void warmupScene() {
        const int warmupSteps = Benchmarks::currentWarmupSteps();
        if (warmupSteps <= 0) {
            return;
        }

        simulation_->setDt(Benchmarks::kDt);
        for (int i = 0; i < warmupSteps; ++i) {
            simulation_->update();
        }
    }

    void prepareForPredict() {
        rebuildScene();
        warmupScene();
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
    int sceneAgeSteps_ = 0;
    int atomCount_ = 0;
};
