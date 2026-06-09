#pragma once
#include <memory>

#include <imgui.h>
#include <glm/vec2.hpp>

namespace Lattice {
    class Simulation;
}
class BaseRenderer;
class CaptureController;
class FileDialogManager;
class Interface;

class SettingsPanel {
public:
    static constexpr ImGuiWindowFlags PANEL_FLAGS =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    void draw(float uiScale, glm::ivec2 windowSize, Lattice::Simulation& simulation, std::unique_ptr<BaseRenderer>& renderer,
              CaptureController& captureController, FileDialogManager& fileDialog, Interface& appInterface);
    void toggle() { visible = !visible; }
    void close() { visible = false; }
    bool isVisible() const { return visible; }

private:
    bool visible = false;
    float animProgress = 0.f;
    bool interfaceScaleEditing_ = false;
    float pendingInterfaceScale_ = 1.0f;
};
