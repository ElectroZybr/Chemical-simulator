#include "SettingsPanel.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "App/AppSignals.h"
#include "App/UserSettings.h"
#include "App/WindowController.h"
#include "App/capture/CaptureController.h"
#include "App/localization/i18n.h"
#include "Lattice/Engine/Simulation.h"
#include "GUI/interface/file_dialog/FileDialogManager.h"
#include "GUI/interface/interface.h"
#include "GUI/interface/style/ComboStyle.h"
#include "Rendering/BaseRenderer.h"
#include "generated/AppVersion.h"

using i18n::operator""_tr;

namespace {
    std::string_view integratorName(Integrator::Scheme scheme) {
        switch (scheme) {
        case Integrator::Scheme::Verlet:
            return "integrator_velocity_verlet"_tr;
        case Integrator::Scheme::KDK:
            return "integrator_kdk"_tr;
        case Integrator::Scheme::RK4:
            return "integrator_runge_kutta_4"_tr;
        case Integrator::Scheme::Langevin:
            return "integrator_langevin"_tr;
        case Integrator::Scheme::Andersen:
            return "integrator_andersen"_tr;;
        default:
            return "integrator_unknown"_tr;
        
        }
    }

    std::string_view speedColorModeName(RenderData::SpeedColorMode mode) {
        switch (mode) {
        case RenderData::SpeedColorMode::AtomColor:
            return "speed_color_normal_coloring"_tr;
        case RenderData::SpeedColorMode::GradientClassic:
            return "speed_color_gradient_coloring"_tr;
        case RenderData::SpeedColorMode::GradientTurbo:
            return "speed_color_turbo_coloring"_tr;
        default:
            return "speed_color_normal_coloring"_tr;
        }
    }

    std::string_view capturePresetName(CaptureSettings::Preset preset) {
        switch (preset) {
        case CaptureSettings::Preset::Ultrafast:
            return "capture_preset_ultrafast"_tr;
        case CaptureSettings::Preset::Veryfast:
            return "capture_preset_veryfast"_tr;
        case CaptureSettings::Preset::Faster:
            return "capture_preset_faster"_tr;
        case CaptureSettings::Preset::Fast:
            return "capture_preset_fast"_tr;
        case CaptureSettings::Preset::Medium:
            return "capture_preset_medium"_tr;
        default:
            return "capture_preset_veryfast"_tr;
        }
    }

    std::string_view capturePixelFormatName(CaptureSettings::PixelFormat pixelFormat) {
        switch (pixelFormat) {
        case CaptureSettings::PixelFormat::Yuv420p:
            return "capture_pixel_format_Yuv420p"_tr;
        case CaptureSettings::PixelFormat::Yuv444p:
            return "capture_pixel_format_Yuv444p"_tr;
        }
        return "capture_pixel_format_Yuv444p"_tr;
    }
}

void SettingsPanel::draw(float uiScale, glm::ivec2 windowSize, Lattice::Simulation& simulation, std::unique_ptr<BaseRenderer>& renderer,
                         CaptureController& captureController, FileDialogManager& fileDialog, Interface& appInterface) {
    float target = visible ? 1.f : 0.f;
    float step = ImGui::GetIO().DeltaTime * 12.f;
    animProgress += (target - animProgress) * std::min(step, 1.f);

    if (animProgress < 0.01f) {
        return;
    }

    if (!renderer || renderer->getRenderDataCount() <= simulation.activeWorldId()) {
        return;
    }

    RenderData& activeRenderData = renderer->getRenderData(simulation.activeWorldId());

    const float panelWidth = 300.f * uiScale;
    const float topOffset = 65.f * uiScale;
    const float panelHeight = static_cast<float>(windowSize.y) - topOffset;

    const float x = -panelWidth + panelWidth * animProgress;

    ImGui::SetNextWindowPos(ImVec2(x, topOffset));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::Begin("##SettingsPanel", nullptr, PANEL_FLAGS);

    ImGui::SeparatorText("imgui_simulation"_tr.data());

    ImGui::TextUnformatted("imgui_gravity"_tr.data());
    ImGui::SameLine();
    glm::vec3 gravity = simulation.world().getGravity();
    if (ImGui::Button("imgui_reset_gravity"_tr.data(), ImVec2(50.f * uiScale, 0.f))) {
        simulation.world().setGravity(glm::vec3(0.0f));
        gravity = simulation.world().getGravity();
    }
    float gx = gravity.x;
    float gy = gravity.y;
    float gz = gravity.z;
    bool gravityChanged = false;
    ImGui::PushItemWidth(150.0f * uiScale);
    const auto drawDragFloat = [](const char* label, float& value, float speed, const char* format) {
        const bool changed = ImGui::DragFloat(label, &value, speed, 0.0f, 0.0f, format);
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        return changed;
    };
    gravityChanged |= drawDragFloat("imgui_gravity_x"_tr.data(), gx, 0.05f, "%.2f");
    gravityChanged |= drawDragFloat("imgui_gravity_y"_tr.data(), gy, 0.05f, "%.2f");
    gravityChanged |= drawDragFloat("imgui_gravity_z"_tr.data(), gz, 0.05f, "%.2f");
    ImGui::PopItemWidth();
    if (gravityChanged) {
        simulation.setGravity(glm::vec3(gx, gy, gz));
    }

    Integrator::Scheme currentIntegrator = simulation.world().getIntegrator().getScheme();
    if (ComboStyle::beginCombo("imgui_integrator"_tr.data(), integratorName(currentIntegrator).data(), 0.0f, uiScale)) {
        const Integrator::Scheme schemes[] = {
            Integrator::Scheme::Verlet,
            Integrator::Scheme::KDK,
            Integrator::Scheme::RK4,
            Integrator::Scheme::Langevin,
            Integrator::Scheme::Andersen
        };

        for (Integrator::Scheme scheme : schemes) {
            const bool isSelected = (scheme == currentIntegrator);
            if (ImGui::Selectable(integratorName(scheme).data(), isSelected)) {
                simulation.world().getIntegrator().setScheme(scheme);
                currentIntegrator = scheme;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (currentIntegrator == Integrator::Scheme::RK4 || currentIntegrator == Integrator::Scheme::Langevin) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.75f, 0.25f, 1.00f));
        ImGui::TextWrapped("imgui_warning_not_implemented_used_as_velocity_verlet"_tr.data(),
                           integratorName(currentIntegrator).data());
        ImGui::PopStyleColor();
    }

    float maxParticleSpeed = simulation.getMaxParticleSpeed();
    ImGui::PushItemWidth(150.0f * uiScale);
    if (ImGui::SliderFloat("imgui_speed_of_light"_tr.data(), &maxParticleSpeed, 0.0f, 100.0f,
                           maxParticleSpeed <= 0.0f ? "imgui_speed_of_light_unlimited"_tr.data() : "%.2f")) {
        simulation.setMaxParticleSpeed(maxParticleSpeed);
    }
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(150.0f * uiScale);
    if (currentIntegrator == Integrator::Scheme::Andersen) {
        float andersenTemperature = simulation.getAndersenTemperature();
        if (ImGui::SliderFloat("Andersen T (K)", &andersenTemperature, 1.0f, 5000.0f, "%.1f",
                               ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic)) {
            simulation.setAndersenTemperature(andersenTemperature);
        }
    }
    else {
        float accelDamping = simulation.getAccelDamping();
        if (ImGui::SliderFloat("imgui_accel_damping"_tr.data(), &accelDamping, 0.0f, 1.0f, "%.3f")) {
            simulation.setAccelDamping(accelDamping);
        }
    }
    ImGui::PopItemWidth();

    float dt = simulation.getDt();
    ImGui::PushItemWidth(150.0f * uiScale);
    if (ImGui::SliderFloat("imgui_time_step"_tr.data(), &dt, 0.0001f, 0.05f, "%.4f",
                           ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic)) {
        simulation.setDt(dt);
    }
    ImGui::PopItemWidth();

    ImGui::SeparatorText("imgui_worlds"_tr.data());
    const std::string activeWorldLabel = std::string("imgui_world_prefix"_tr) + std::to_string(simulation.activeWorldId());
    if (ComboStyle::beginCombo("##active_world", activeWorldLabel.c_str(), 180.0f * uiScale, uiScale, ImGuiComboFlags_HeightLargest)) {
        for (Lattice::Simulation::WorldId worldId = 0; worldId < simulation.worldCount(); ++worldId) {
            const std::string worldLabel = std::string("imgui_world_prefix"_tr) + std::to_string(worldId);
            const bool isSelected = worldId == simulation.activeWorldId();
            if (ImGui::Selectable(worldLabel.c_str(), isSelected)) {
                simulation.setActiveWorld(worldId);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("imgui_create_world"_tr.data(), ImVec2(150.0f * uiScale, 0.0f))) {
        constexpr float worldGap = 20.0f;
        const glm::vec3 newWorldSize = simulation.world().getWorldSize();
        glm::vec3 newWorldOffset = simulation.world().getRenderOffset();
        float rightEdge = newWorldOffset.x + newWorldSize.x;
        for (Lattice::Simulation::WorldId worldId = 0; worldId < simulation.worldCount(); ++worldId) {
            const World& world = simulation.worldAt(worldId);
            rightEdge = std::max(rightEdge, world.getRenderOffset().x + world.getWorldSize().x);
        }
        newWorldOffset.x = rightEdge + worldGap;
        const Lattice::Simulation::WorldId newWorldId = simulation.createWorld(newWorldSize, newWorldOffset);
        simulation.setActiveWorld(newWorldId);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(simulation.worldCount() <= 1);
    if (ImGui::Button("imgui_delete_world"_tr.data(), ImVec2(90.0f * uiScale, 0.0f))) {
        simulation.removeWorld(simulation.activeWorldId());
    }
    ImGui::EndDisabled();

    glm::vec3 renderOffset = simulation.world().getRenderOffset();
    ImGui::PushItemWidth(150.0f * uiScale);
    bool offsetChanged = false;
    offsetChanged |= drawDragFloat("imgui_offset_x"_tr.data(), renderOffset.x, 0.25f, "%.1f");
    offsetChanged |= drawDragFloat("imgui_offset_y"_tr.data(), renderOffset.y, 0.25f, "%.1f");
    offsetChanged |= drawDragFloat("imgui_offset_z"_tr.data(), renderOffset.z, 0.25f, "%.1f");
    if (offsetChanged) {
        simulation.world().setRenderOffset(renderOffset);
    }
    ImGui::PopItemWidth();

    glm::vec3 targetBoxSize = simulation.world().getTargetWorldSize();
    ImGui::PushItemWidth(150.0f * uiScale);
    bool boxSizeChanged = false;
    const auto drawBoxSizeDrag = [&](const char* label, const char* id, float& value) {
        float clampedValue = std::max(value, 1.0f);
        const bool changed = ImGui::DragFloat(id, &clampedValue, 0.5f, 1.0f, FLT_MAX, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        value = std::max(clampedValue, 1.0f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
        return changed;
    };
    boxSizeChanged |= drawBoxSizeDrag("Size X", "##settings_box_size_x", targetBoxSize.x);
    boxSizeChanged |= drawBoxSizeDrag("Size Y", "##settings_box_size_y", targetBoxSize.y);
    boxSizeChanged |= drawBoxSizeDrag("Size Z", "##settings_box_size_z", targetBoxSize.z);
    ImGui::PopItemWidth();
    if (boxSizeChanged) {
        AppSignals::UI::NewTargetBoxSize.emit(targetBoxSize);
    }

    bool bondFormationEnabled = simulation.world().isBondFormationEnabled();
    if (ImGui::Checkbox("imgui_bond_formation"_tr.data(), &bondFormationEnabled)) {
        simulation.world().setBondFormationEnabled(bondFormationEnabled);
    }

    bool ljEnabled = simulation.world().isLJEnabled();
    if (ImGui::Checkbox("imgui_lj"_tr.data(), &ljEnabled)) {
        simulation.world().setLJEnabled(ljEnabled);
    }
    ImGui::SameLine();
    bool coulombEnabled = simulation.world().isCoulombEnabled();
    if (ImGui::Checkbox("imgui_coulomb"_tr.data(), &coulombEnabled)) {
        simulation.world().setCoulombEnabled(coulombEnabled);
    }
    bool coulombLongRangeEnabled = simulation.world().isCoulombLongRangeEnabled();
    if (ImGui::Checkbox("imgui_coulomb_long_range"_tr.data(), &coulombLongRangeEnabled)) {
        simulation.world().setCoulombLongRangeEnabled(coulombLongRangeEnabled);
    }

    ImGui::SeparatorText("imgui_render"_tr.data());
    ImGui::Checkbox("imgui_atoms"_tr.data(), &activeRenderData.drawAtoms);
    ImGui::Checkbox("imgui_grid"_tr.data(), &activeRenderData.drawGrid);
    ImGui::Checkbox("imgui_connections"_tr.data(), &activeRenderData.drawBonds);
    ImGui::Checkbox("imgui_box"_tr.data(), &activeRenderData.drawBox);
    ImGui::Checkbox("imgui_memory_order"_tr.data(), &activeRenderData.drawMemoryOrder);

    ImGui::TextUnformatted("imgui_color_scheme"_tr.data());
    RenderData::SpeedColorMode speedMode = activeRenderData.speedColorMode;
    if (ComboStyle::beginCombo("imgui_speed_color_mode"_tr.data(), speedColorModeName(speedMode).data(), 220.0f * uiScale, uiScale, ImGuiComboFlags_HeightLargest)) {
        const RenderData::SpeedColorMode modes[] = {
            RenderData::SpeedColorMode::AtomColor,
            RenderData::SpeedColorMode::GradientClassic,
            RenderData::SpeedColorMode::GradientTurbo,
        };

        for (RenderData::SpeedColorMode mode : modes) {
            const bool isSelected = (mode == speedMode);
            if (ImGui::Selectable(speedColorModeName(mode).data(), isSelected)) {
                speedMode = mode;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    activeRenderData.speedColorMode = speedMode;

    ImGui::TextUnformatted("imgui_max_gradien_velocity"_tr.data());

    static float manualSpeedGradientMax = 5.0f;
    bool autoSpeedGradient = activeRenderData.speedGradientMax <= 0.0f;
    const bool gradientModeEnabled = activeRenderData.speedColorMode != RenderData::SpeedColorMode::AtomColor;
    if (!autoSpeedGradient) {
        manualSpeedGradientMax = activeRenderData.speedGradientMax;
    }

    ImGui::PushItemWidth(180.0f * uiScale);
    ImGui::BeginDisabled(autoSpeedGradient || !gradientModeEnabled);
    if (ImGui::SliderFloat("imgui_speed_gradient_max_slider"_tr.data(), &manualSpeedGradientMax, 0.1f, 10.0f, "%.2f")) {
        activeRenderData.speedGradientMax = manualSpeedGradientMax;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!gradientModeEnabled);
    if (ImGui::Checkbox("imgui_auto_speed_gradien"_tr.data(), &autoSpeedGradient)) {
        activeRenderData.speedGradientMax = autoSpeedGradient ? 0.0f : manualSpeedGradientMax;
    }
    ImGui::EndDisabled();
    ImGui::PopItemWidth();

    ImGui::SeparatorText("Interface");
    const float currentInterfaceScale = std::round(appInterface.uiScaleMultiplier() * 10.0f) / 10.0f;
    if (!interfaceScaleEditing_) {
        pendingInterfaceScale_ = currentInterfaceScale;
    }

    ImGui::PushItemWidth(180.0f * uiScale);
    if (ImGui::SliderFloat("Interface scale", &pendingInterfaceScale_, StyleManager::kMinUiScale, StyleManager::kMaxUiScale, "%.1f",
                           ImGuiSliderFlags_AlwaysClamp)) {
        pendingInterfaceScale_ = std::round(pendingInterfaceScale_ * 10.0f) / 10.0f;
        interfaceScaleEditing_ = true;
    }
    ImGui::PopItemWidth();
    if (interfaceScaleEditing_ && ImGui::IsItemDeactivatedAfterEdit()) {
        appInterface.setUiScaleMultiplier(pendingInterfaceScale_);
        interfaceScaleEditing_ = false;
    }
    else if (interfaceScaleEditing_ && !ImGui::IsItemActive()) {
        pendingInterfaceScale_ = currentInterfaceScale;
        interfaceScaleEditing_ = false;
    }

    ImGui::SeparatorText("imgui_neighbour_list"_tr.data());
    int cellSize = simulation.world().getGridCellSize();
    if (ImGui::SliderInt("imgui_cell_size"_tr.data(), &cellSize, 1, 32)) {
        simulation.setBoxSize(simulation.world().getWorldSize(), cellSize);
    }

    float cutoff = simulation.getNeighborListCutoff();
    if (ImGui::SliderFloat("imgui_cutoff_nl"_tr.data(), &cutoff, 0.5f, 20.0f, "%.2f")) {
        simulation.setNeighborListCutoff(cutoff);
    }

    float skin = simulation.getNeighborListSkin();
    if (ImGui::SliderFloat("imgui_skin_nl"_tr.data(), &skin, 0.1f, 10.0f, "%.2f")) {
        simulation.setNeighborListSkin(skin);
    }

    if (captureController.isAvailable()) {
        ImGui::SeparatorText("imgui_write"_tr.data());
        CaptureSettings captureSettings = captureController.settings();
        const bool recordingActive = captureController.isRecording();
        const std::string captureDir = captureController.outputDirectory().string();

        ImGui::TextUnformatted("imgui_video_saving_folder"_tr.data());
        std::array<char, 512> captureDirBuffer{};
        std::snprintf(captureDirBuffer.data(), captureDirBuffer.size(), "%s", captureDir.data());
        const float browseButtonWidth = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseButtonWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputText("imgui_capture_dir"_tr.data(), captureDirBuffer.data(), captureDirBuffer.size(),
                         ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("imgui_capture_dir_browse"_tr.data(), ImVec2(browseButtonWidth, 0.0f))) {
            fileDialog.openCaptureDirectory(captureDir);
        }

        ImGui::BeginDisabled(recordingActive);

        if (ImGui::BeginTable("imgui_capture_settings_table"_tr.data(), 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-FLT_MIN);
            int captureFps = captureSettings.fps;
            if (ImGui::SliderInt("imgui_fps_capture"_tr.data(), &captureFps, 10, 60)) {
                captureSettings.fps = captureFps;
                captureController.setSettings(captureSettings);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            int crf = captureSettings.crf;
            if (ImGui::SliderInt("imgui_crf_capture"_tr.data(), &crf, 12, 30)) {
                captureSettings.crf = crf;
                captureController.setSettings(captureSettings);
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            CaptureSettings::Preset preset = captureSettings.preset;
            if (ComboStyle::beginCombo("imgui_preset_capture"_tr.data(), capturePresetName(preset).data(), -FLT_MIN, uiScale)) {
                const CaptureSettings::Preset presets[] = {
                    CaptureSettings::Preset::Ultrafast, CaptureSettings::Preset::Veryfast, CaptureSettings::Preset::Faster,
                    CaptureSettings::Preset::Fast,      CaptureSettings::Preset::Medium,
                };

                for (CaptureSettings::Preset candidate : presets) {
                    const bool isSelected = (candidate == preset);
                    if (ImGui::Selectable(capturePresetName(candidate).data(), isSelected)) {
                        captureSettings.preset = candidate;
                        captureController.setSettings(captureSettings);
                        preset = candidate;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(1);
            CaptureSettings::PixelFormat pixelFormat = captureSettings.pixelFormat;
            if (ComboStyle::beginCombo("imgui_color_capture"_tr.data(), capturePixelFormatName(pixelFormat).data(), -FLT_MIN,
                                       uiScale)) {
                const CaptureSettings::PixelFormat pixelFormats[] = {
                    CaptureSettings::PixelFormat::Yuv444p,
                    CaptureSettings::PixelFormat::Yuv420p,
                };

                for (CaptureSettings::PixelFormat candidate : pixelFormats) {
                    const bool isSelected = (candidate == pixelFormat);
                    if (ImGui::Selectable(capturePixelFormatName(candidate).data(), isSelected)) {
                        captureSettings.pixelFormat = candidate;
                        captureController.setSettings(captureSettings);
                        pixelFormat = candidate;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::EndTable();
        }

        ImGui::EndDisabled();
    }

    ImGui::SeparatorText("imgui_misc"_tr.data());
    const bool isFullscreen = WindowController::snapshot().fullscreen;
    const char* fullscreenButtonLabel = isFullscreen ? "imgui_windowed"_tr.data() : "imgui_fullscreen"_tr.data();
    if (ImGui::Button(fullscreenButtonLabel, ImVec2(190.0f * uiScale, 0.0f))) {
        WindowController::toggleFullscreen();
    }
    ImGui::SameLine();
    const float languageButtonWidth = 86.0f * uiScale;
    if (ImGui::Button("imgui_language_toggle"_tr.data(), ImVec2(languageButtonWidth, 0.0f))) {
        i18n::toggle();
    }

    if (ImGui::Button("imgui_reset_settings"_tr.data(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        const UserSettings defaults;
        captureController.setOutputDirectory(defaults.captureOutputDirectory);
        captureController.setSettings(defaults.captureSettings);

        activeRenderData.drawAtoms = defaults.rendererDrawAtoms;
        activeRenderData.drawGrid = defaults.rendererDrawGrid;
        activeRenderData.drawBonds = defaults.rendererDrawBonds;
        activeRenderData.drawBox = defaults.rendererDrawBox;
        activeRenderData.drawMemoryOrder = defaults.rendererDrawMemoryOrder;
        appInterface.setUiScaleMultiplier(defaults.interfaceScale);
        activeRenderData.speedColorMode = defaults.rendererSpeedColorMode;
        activeRenderData.speedGradientMax = defaults.rendererSpeedGradientMax;

        simulation.world().getIntegrator().setScheme(defaults.simulationIntegrator);
        simulation.setBondFormationEnabled(defaults.simulationBondFormationEnabled);
        simulation.setLJEnabled(defaults.simulationLJEnabled);
        simulation.setCoulombEnabled(defaults.simulationCoulombEnabled);
        simulation.world().setCoulombLongRangeEnabled(defaults.simulationCoulombLongRangeEnabled);
    }

    const float exitButtonWidth = ImGui::GetContentRegionAvail().x;
    const std::string versionText =
        std::string("version_text_pre"_tr) + LATTICELAB_VERSION_STRING + std::string("version_text_after"_tr);
    const float versionWidth = ImGui::CalcTextSize(versionText.c_str()).x;
    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing();
    const float remaining = ImGui::GetContentRegionAvail().y - footerHeight;
    if (remaining > 0.0f) {
        ImGui::Dummy(ImVec2(0.0f, remaining));
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.76f, 0.80f, 0.85f));
    ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - versionWidth) * 0.5f));
    ImGui::TextUnformatted(versionText.c_str());
    ImGui::PopStyleColor();

    if (ImGui::Button("imgui_exit_button"_tr.data(), ImVec2(exitButtonWidth, 0.0f))) {
        AppSignals::UI::ExitApplication.emit();
    }

    ImGui::End();
}
