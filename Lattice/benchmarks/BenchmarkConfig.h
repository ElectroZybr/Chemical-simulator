#pragma once

#include <algorithm>
#include <string_view>

#include "SceneBuilder.h"

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
