#pragma once

#include <algorithm>
#include <memory>
#include <string_view>

#include <benchmark/benchmark.h>

#include "SceneBuilder.h"
#include "Lattice/Engine/Simulation.h"
#include "Lattice/Plugins/ClassicMD/Integrators/StepOps.h"
#include "Lattice/Plugins/ClassicMD/Integrators/Verlet.h"

namespace Benchmarks {
    constexpr double kDt = 0.01;
    constexpr int kTemporalDegradationExtent = 25;

    enum class DegradationCriterion {
        Size,
        Time,
    };

    inline SceneKind sceneFromString(std::string_view value) {
        if (value == "gas") {
            return SceneKind::Gas;
        }
        return SceneKind::Crystal;
    }

    inline SceneKind& selectedScene() {
        static SceneKind scene = SceneKind::Crystal;
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
        case SceneKind::Gas:
        case SceneKind::Crystal:
            return sceneExtent * sceneExtent * sceneExtent;
        }

        return sceneExtent;
    }
}

namespace Benchmarks {

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
    StepContext makeStepData() {
        World& world = simulation_->world();
        return StepContext{
            .world = world,
            .forceField = simulation_->forceField(),
            .neighborList = simulation_->neighborList(),
            .thermostat = world.getThermostat().activeThermostat(),
            .chemistryData = world.getState().chemistryData,
            .allowBondFormation = simulation_->isBondFormationEnabled(),
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
        StepContext stepData = makeStepData();
        StepOps::computeForces(stepData);
    }

    void prepareNeighborList() { simulation_->neighborList().build(simulation_->atoms(), simulation_->world()); }

    void prepareForCorrect() {
        prepareForPredict();
        StepContext stepData = makeStepData();
        StepOps::predictAndSync(stepData, &Verlet::predict);
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

using FixtureMT = Fixture;

} // namespace Benchmarks

using Fixture = Benchmarks::Fixture;
using FixtureMT = Benchmarks::Fixture;

using FixtureMT = Benchmarks::Fixture;

using FixtureMT = Fixture;