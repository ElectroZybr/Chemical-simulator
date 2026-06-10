#pragma once

#include <memory>

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

    private:
        void trackIOPanel(CaptureController& captureController, UiState& uiState, Lattice::Simulation& simulation, SceneViewport& renderer);
        void trackToolsPanel(Lattice::Simulation& simulation, SceneViewport& renderer);
        void trackSettingsPanel(bool& exitRequested);
        void trackKeyboard(Lattice::Simulation& simulation);
        void trackSimControlPanel(Lattice::Simulation& simulation);
    };
}
