#pragma once

#include <memory>

#include <GLFW/glfw3.h>

#include "Signals/Signals.h"

namespace Lattice {
    class Simulation;
}
class CaptureController;
class SceneViewport;
struct UiState;

namespace AppActions {
    class Handler : public Signals::Trackable {
    public:
        Handler(GLFWwindow* window, CaptureController& captureController, Lattice::Simulation& simulation, SceneViewport& renderer, UiState& uiState);

    private:
        void trackIOPanel(CaptureController& captureController, UiState& uiState, Lattice::Simulation& simulation, SceneViewport& renderer);
        void trackToolsPanel(Lattice::Simulation& simulation, SceneViewport& renderer);
        void trackSettingsPanel(GLFWwindow* window);
        void trackKeyboard(Lattice::Simulation& simulation);
        void trackSimControlPanel(Lattice::Simulation& simulation);
    };
}
