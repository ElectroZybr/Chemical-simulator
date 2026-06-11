#pragma once

#include <memory>

#include <glm/vec3.hpp>

#include "App/Signals.h"

namespace Lattice {
    class Simulation;
}
class CaptureController;
class SceneViewport;
struct UiState;

namespace AppActions {
    class Handler : public Signals::Trackable {
    public:
        Handler(CaptureController& captureController, Lattice::Simulation& simulation, SceneViewport& renderer, UiState& uiState, bool& exitRequested);
        void updateSimulationStep(Lattice::Simulation& simulation);

    private:
        void trackIOPanel(CaptureController& captureController, UiState& uiState, Lattice::Simulation& simulation, SceneViewport& renderer);
        void trackToolsPanel(Lattice::Simulation& simulation, SceneViewport& renderer);
        void trackSettingsPanel(bool& exitRequested);
        void trackKeyboard(Lattice::Simulation& simulation);
        void trackSimControlPanel(Lattice::Simulation& simulation);
        void applySmoothResizeStep(Lattice::Simulation& simulation);

        glm::vec3 smoothResizeTarget_{0.0f};
        float smoothResizeMaxSpeed_ = 0.0f;
        bool smoothResizeActive_ = false;
    };
}
