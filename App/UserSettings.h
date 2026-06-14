#pragma once

#include <filesystem>

#include "App/AppPaths.h"
#include "App/capture/CaptureSettings.h"
#include "GUI/interface/style/StyleManager.h"
#include "Lattice/Engine/physics/Integrator.h"
#include "Rendering/RenderData.h"

struct UserSettings {
    struct WindowState {
        bool fullscreen = false;
        bool maximized = true;
        int monitorIndex = 0;
        int x = 160;
        int y = 120;
        int width = 1920;
        int height = 1080;
    };

    std::filesystem::path captureOutputDirectory = "captures";
    std::filesystem::path scenesDirectory = AppPaths::kDefaultScenesDirectory;
    CaptureSettings captureSettings{};
    WindowState windowState{};

    bool rendererUse3D = true;
    bool rendererDrawAtoms = true;
    bool rendererDrawGrid = false;
    bool rendererDrawVectorField = false;
    bool rendererDrawFieldArrows = false;
    bool rendererDrawFieldContours = false;
    bool rendererFieldAutoScale = true;
    bool rendererDrawBonds = false;
    bool rendererDrawBox = true;
    bool rendererDrawMemoryOrder = false;
    float interfaceScale = StyleManager::kDefaultUiScale;
    RenderData::SpeedColorMode rendererSpeedColorMode = RenderData::SpeedColorMode::AtomColor;
    float rendererSpeedGradientMax = 5.0f;
    float rendererFieldPotentialScale = 50.0f;
    float rendererFieldCellSize = 1.0f;
    float rendererFieldSmoothing = 1.0f;
    float rendererFieldContourStep = 0.2f;

    Integrator::Scheme simulationIntegrator = Integrator::Scheme::Verlet;
    bool simulationBondFormationEnabled = true;
    bool simulationLJEnabled = true;
    bool simulationCoulombEnabled = true;
    bool simulationCoulombLongRangeEnabled = true;
};

class UserSettingsIO {
public:
    static UserSettings load(const std::filesystem::path& path = defaultPath());
    static void save(const UserSettings& settings, const std::filesystem::path& path = defaultPath());

    static std::filesystem::path defaultPath();
};
