#pragma once

#include <filesystem>

#include "App/AppPaths.h"
#include "App/capture/CaptureSettings.h"
#include "Lattice/Engine/physics/Integrator.h"
#include "Rendering/RenderData.h"

struct UserSettings {
    std::filesystem::path captureOutputDirectory = "captures";
    std::filesystem::path scenesDirectory = AppPaths::kDefaultScenesDirectory;
    CaptureSettings captureSettings{};

    bool rendererDrawGrid = false;
    bool rendererDrawBonds = true;
    bool rendererDrawBox = true;
    RenderData::SpeedColorMode rendererSpeedColorMode = RenderData::SpeedColorMode::AtomColor;
    float rendererSpeedGradientMax = 5.0f;

    Integrator::Scheme simulationIntegrator = Integrator::Scheme::Verlet;
    bool simulationBondFormationEnabled = false;
    bool simulationLJEnabled = true;
    bool simulationCoulombEnabled = true;
};

class UserSettingsIO {
public:
    static UserSettings load(const std::filesystem::path& path = defaultPath());
    static void save(const UserSettings& settings, const std::filesystem::path& path = defaultPath());

    static std::filesystem::path defaultPath();
};
