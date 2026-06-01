#pragma once
#include <chrono>
#include <filesystem>
#include <memory>

#include <GLFW/glfw3.h>

#include "GUI/interface/UiState.h"
#include "GUI/interface/file_dialog/FileDialogManager.h"
#include "GUI/interface/font_manager/FontManager.h"
#include "GUI/interface/panels/debug/DebugPanel.h"
#include "GUI/interface/panels/io/ioPanel.h"
#include "GUI/interface/panels/periodic/PeriodicPanel.h"
#include "GUI/interface/panels/settings/SettingsPanel.h"
#include "GUI/interface/panels/sim_control/SimControlPanel.h"
#include "GUI/interface/panels/stats/StatsPanel.h"
#include "GUI/interface/panels/tools/SideToolsPanel.h"
#include "GUI/interface/panels/tools/ToolsPanel.h"
#include "GUI/interface/style/StyleManager.h"

class BaseRenderer;
namespace Lattice {
    class Simulation;
}

class Interface {
public:
    Interface(GLFWwindow* w, Lattice::Simulation& s, std::unique_ptr<BaseRenderer>& r, class CaptureController& c);

    int init();
    void shutdown();
    int update();
    void draw(BaseRenderer& renderer);
    UiState& state();
    const UiState& state() const;
    void setScenesDirectory(std::filesystem::path scenesDirectory);
    [[nodiscard]] const std::filesystem::path& scenesDirectory() const;

    FontManager fontManager;

    DebugPanel debugPanel;
    FileDialogManager fileDialog;
    StyleManager styleManager;
    ToolsPanel toolsPanel;
    IOPanel ioPanel;
    SideToolsPanel sideToolsPanel;
    SimControlPanel simControlPanel;
    PeriodicPanel periodicPanel;
    StatsPanel statsPanel;
    SettingsPanel settingsPanel;

private:
    GLFWwindow* window_;
    Lattice::Simulation* simulation_;
    std::unique_ptr<BaseRenderer>* renderer_;
    class CaptureController* captureController_;
    std::chrono::high_resolution_clock::time_point lastTime_;
    UiState uiState_;
};
