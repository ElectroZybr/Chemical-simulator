#pragma once
#include <memory>

#include <imgui.h>

#include "Lattice/Engine/math/Vec2.h"

namespace Lattice {
    class Simulation;
}
class BaseRenderer;
class CaptureController;
class FileDialogManager;

class SettingsPanel {
public:
    static constexpr ImGuiWindowFlags PANEL_FLAGS =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    void draw(float uiScale, Vec2i windowSize, Lattice::Simulation& simulation, std::unique_ptr<BaseRenderer>& renderer,
              CaptureController& captureController, FileDialogManager& fileDialog);
    void toggle() { visible = !visible; }
    void close() { visible = false; }
    bool isVisible() const { return visible; }

private:
    bool visible = false;
    float animProgress = 0.f;
};
