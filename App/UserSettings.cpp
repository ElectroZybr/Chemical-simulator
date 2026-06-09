#include "UserSettings.h"

#include <algorithm>
#include <fstream>
#include <string>

namespace {
    const char* presetToString(CaptureSettings::Preset preset) {
        switch (preset) {
        case CaptureSettings::Preset::Ultrafast:
            return "ultrafast";
        case CaptureSettings::Preset::Veryfast:
            return "veryfast";
        case CaptureSettings::Preset::Faster:
            return "faster";
        case CaptureSettings::Preset::Fast:
            return "fast";
        case CaptureSettings::Preset::Medium:
            return "medium";
        }
        return "veryfast";
    }

    CaptureSettings::Preset presetFromString(const std::string& value) {
        if (value == "ultrafast") {
            return CaptureSettings::Preset::Ultrafast;
        }
        if (value == "veryfast") {
            return CaptureSettings::Preset::Veryfast;
        }
        if (value == "faster") {
            return CaptureSettings::Preset::Faster;
        }
        if (value == "fast") {
            return CaptureSettings::Preset::Fast;
        }
        if (value == "medium") {
            return CaptureSettings::Preset::Medium;
        }
        return CaptureSettings::Preset::Veryfast;
    }

    const char* pixelFormatToString(CaptureSettings::PixelFormat pixelFormat) {
        switch (pixelFormat) {
        case CaptureSettings::PixelFormat::Yuv420p:
            return "yuv420p";
        case CaptureSettings::PixelFormat::Yuv444p:
            return "yuv444p";
        }
        return "yuv444p";
    }

    CaptureSettings::PixelFormat pixelFormatFromString(const std::string& value) {
        if (value == "yuv420p") {
            return CaptureSettings::PixelFormat::Yuv420p;
        }
        return CaptureSettings::PixelFormat::Yuv444p;
    }

    const char* speedColorModeToString(RenderData::SpeedColorMode mode) {
        switch (mode) {
        case RenderData::SpeedColorMode::AtomColor:
            return "atom_color";
        case RenderData::SpeedColorMode::GradientClassic:
            return "gradient_classic";
        case RenderData::SpeedColorMode::GradientTurbo:
            return "gradient_turbo";
        }
        return "atom_color";
    }

    RenderData::SpeedColorMode speedColorModeFromString(const std::string& value) {
        if (value == "gradient_classic") {
            return RenderData::SpeedColorMode::GradientClassic;
        }
        if (value == "gradient_turbo") {
            return RenderData::SpeedColorMode::GradientTurbo;
        }
        return RenderData::SpeedColorMode::AtomColor;
    }

    const char* integratorToString(Integrator::Scheme scheme) {
        switch (scheme) {
        case Integrator::Scheme::Verlet:
            return "verlet";
        case Integrator::Scheme::KDK:
            return "kdk";
        case Integrator::Scheme::RK4:
            return "rk4";
        case Integrator::Scheme::Langevin:
            return "langevin";
        }
        return "verlet";
    }

    Integrator::Scheme integratorFromString(const std::string& value) {
        if (value == "kdk") {
            return Integrator::Scheme::KDK;
        }
        if (value == "rk4") {
            return Integrator::Scheme::RK4;
        }
        if (value == "langevin") {
            return Integrator::Scheme::Langevin;
        }
        return Integrator::Scheme::Verlet;
    }
}

std::filesystem::path UserSettingsIO::defaultPath() { return "user_settings.cfg"; }

UserSettings UserSettingsIO::load(const std::filesystem::path& path) {
    UserSettings settings;

    std::ifstream file(path);
    if (!file.is_open()) {
        return settings;
    }

    std::string tag;
    while (file >> tag) {
        if (tag == "capture_output_dir") {
            std::string value;
            if (file >> std::ws && std::getline(file, value) && !value.empty()) {
                settings.captureOutputDirectory = value;
            }
        }
        else if (tag == "scenes_dir") {
            std::string value;
            if (file >> std::ws && std::getline(file, value) && !value.empty()) {
                settings.scenesDirectory = value;
            }
        }
        else if (tag == "capture_fps") {
            file >> settings.captureSettings.fps;
        }
        else if (tag == "capture_crf") {
            file >> settings.captureSettings.crf;
        }
        else if (tag == "capture_preset") {
            std::string value;
            file >> value;
            settings.captureSettings.preset = presetFromString(value);
        }
        else if (tag == "capture_pixel_format") {
            std::string value;
            file >> value;
            settings.captureSettings.pixelFormat = pixelFormatFromString(value);
        }
        else if (tag == "window_fullscreen") {
            file >> settings.windowState.fullscreen;
        }
        else if (tag == "window_maximized") {
            file >> settings.windowState.maximized;
        }
        else if (tag == "window_monitor_index") {
            file >> settings.windowState.monitorIndex;
        }
        else if (tag == "window_x") {
            file >> settings.windowState.x;
        }
        else if (tag == "window_y") {
            file >> settings.windowState.y;
        }
        else if (tag == "window_width") {
            file >> settings.windowState.width;
        }
        else if (tag == "window_height") {
            file >> settings.windowState.height;
        }
        else if (tag == "renderer_use_3d") {
            file >> settings.rendererUse3D;
        }
        else if (tag == "renderer_draw_grid") {
            file >> settings.rendererDrawGrid;
        }
        else if (tag == "renderer_draw_atoms") {
            file >> settings.rendererDrawAtoms;
        }
        else if (tag == "renderer_draw_bonds") {
            file >> settings.rendererDrawBonds;
        }
        else if (tag == "renderer_draw_box") {
            file >> settings.rendererDrawBox;
        }
        else if (tag == "renderer_draw_memory_order") {
            file >> settings.rendererDrawMemoryOrder;
        }
        else if (tag == "interface_scale") {
            file >> settings.interfaceScale;
        }
        else if (tag == "renderer_speed_color_mode") {
            std::string value;
            file >> value;
            settings.rendererSpeedColorMode = speedColorModeFromString(value);
        }
        else if (tag == "renderer_speed_gradient_max") {
            file >> settings.rendererSpeedGradientMax;
        }
        else if (tag == "simulation_integrator") {
            std::string value;
            file >> value;
            settings.simulationIntegrator = integratorFromString(value);
        }
        else if (tag == "simulation_bond_formation") {
            file >> settings.simulationBondFormationEnabled;
        }
        else if (tag == "simulation_lj_enabled") {
            file >> settings.simulationLJEnabled;
        }
        else if (tag == "simulation_coulomb_enabled") {
            file >> settings.simulationCoulombEnabled;
        }
        else if (tag == "simulation_coulomb_long_range_enabled") {
            file >> settings.simulationCoulombLongRangeEnabled;
        }
        else {
            std::string ignoredLine;
            std::getline(file, ignoredLine);
        }
    }

    if (settings.captureSettings.fps < 1) {
        settings.captureSettings.fps = 30;
    }
    settings.captureSettings.crf = std::clamp(settings.captureSettings.crf, 0, 51);
    if (settings.captureOutputDirectory.empty()) {
        settings.captureOutputDirectory = "captures";
    }
    if (settings.scenesDirectory.empty()) {
        settings.scenesDirectory = AppPaths::kDefaultScenesDirectory;
    }
    settings.interfaceScale = std::clamp(settings.interfaceScale, StyleManager::kMinUiScale, StyleManager::kMaxUiScale);
    settings.rendererSpeedGradientMax = std::max(0.0f, settings.rendererSpeedGradientMax);
    settings.windowState.monitorIndex = std::max(0, settings.windowState.monitorIndex);
    settings.windowState.width = std::max(320, settings.windowState.width);
    settings.windowState.height = std::max(240, settings.windowState.height);

    return settings;
}

void UserSettingsIO::save(const UserSettings& settings, const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return;
    }

    file << "capture_output_dir " << settings.captureOutputDirectory.string() << "\n";
    file << "scenes_dir " << settings.scenesDirectory.string() << "\n";
    file << "capture_fps " << settings.captureSettings.fps << "\n";
    file << "capture_crf " << settings.captureSettings.crf << "\n";
    file << "capture_preset " << presetToString(settings.captureSettings.preset) << "\n";
    file << "capture_pixel_format " << pixelFormatToString(settings.captureSettings.pixelFormat) << "\n";
    file << "window_fullscreen " << static_cast<int>(settings.windowState.fullscreen) << "\n";
    file << "window_maximized " << static_cast<int>(settings.windowState.maximized) << "\n";
    file << "window_monitor_index " << settings.windowState.monitorIndex << "\n";
    file << "window_x " << settings.windowState.x << "\n";
    file << "window_y " << settings.windowState.y << "\n";
    file << "window_width " << settings.windowState.width << "\n";
    file << "window_height " << settings.windowState.height << "\n";
    file << "renderer_use_3d " << static_cast<int>(settings.rendererUse3D) << "\n";
    file << "renderer_draw_atoms " << static_cast<int>(settings.rendererDrawAtoms) << "\n";
    file << "renderer_draw_grid " << static_cast<int>(settings.rendererDrawGrid) << "\n";
    file << "renderer_draw_bonds " << static_cast<int>(settings.rendererDrawBonds) << "\n";
    file << "renderer_draw_box " << static_cast<int>(settings.rendererDrawBox) << "\n";
    file << "renderer_draw_memory_order " << static_cast<int>(settings.rendererDrawMemoryOrder) << "\n";
    file << "interface_scale " << settings.interfaceScale << "\n";
    file << "renderer_speed_color_mode " << speedColorModeToString(settings.rendererSpeedColorMode) << "\n";
    file << "renderer_speed_gradient_max " << settings.rendererSpeedGradientMax << "\n";
    file << "simulation_integrator " << integratorToString(settings.simulationIntegrator) << "\n";
    file << "simulation_bond_formation " << static_cast<int>(settings.simulationBondFormationEnabled) << "\n";
    file << "simulation_lj_enabled " << static_cast<int>(settings.simulationLJEnabled) << "\n";
    file << "simulation_coulomb_enabled " << static_cast<int>(settings.simulationCoulombEnabled) << "\n";
    file << "simulation_coulomb_long_range_enabled " << static_cast<int>(settings.simulationCoulombLongRangeEnabled) << "\n";
}
