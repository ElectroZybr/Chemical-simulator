#include "SettingsPanel.h"

#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "App/AppSignals.h"
#include "App/UserSettings.h"
#include "App/capture/CaptureController.h"
#include "App/localization/i18n.h"
#include "Engine/Simulation.h"
#include "GUI/interface/file_dialog/FileDialogManager.h"
#include "GUI/interface/style/ComboStyle.h"
#include "Rendering/BaseRenderer.h"
#include "generated/AppVersion.h"

namespace {
    std::string_view integratorName(Integrator::Scheme scheme) {
        switch (scheme) {
        case Integrator::Scheme::Verlet:
            return i18n::tr("integrator_velocity_verlet");
        case Integrator::Scheme::KDK:
            return i18n::tr("integrator_kdk");
        case Integrator::Scheme::RK4:
            return i18n::tr("integrator_runge_kutta_4");
        case Integrator::Scheme::Langevin:
            return i18n::tr("integrator_langevin");
        default:
            return i18n::tr("integrator_unknown");
        }
    }

    std::string_view speedColorModeName(IRenderer::SpeedColorMode mode) {
        switch (mode) {
        case IRenderer::SpeedColorMode::AtomColor:
            return i18n::tr("speed_color_normal_coloring");
        case IRenderer::SpeedColorMode::GradientClassic:
            return i18n::tr("speed_color_gradient_coloring");
        case IRenderer::SpeedColorMode::GradientTurbo:
            return i18n::tr("speed_color_turbo_coloring");
        default:
            return i18n::tr("speed_color_normal_coloring");
        }
    }

    std::string_view capturePresetName(CaptureSettings::Preset preset) {
        switch (preset) {
        case CaptureSettings::Preset::Ultrafast:
            return i18n::tr("capture_preset_ultrafast");
        case CaptureSettings::Preset::Veryfast:
            return i18n::tr("capture_preset_veryfast");
        case CaptureSettings::Preset::Faster:
            return i18n::tr("capture_preset_faster");
        case CaptureSettings::Preset::Fast:
            return i18n::tr("capture_preset_fast");
        case CaptureSettings::Preset::Medium:
            return i18n::tr("capture_preset_medium");
        default:
            return i18n::tr("capture_preset_veryfast");
        }
    }

    std::string_view capturePixelFormatName(CaptureSettings::PixelFormat pixelFormat) {
        switch (pixelFormat) {
        case CaptureSettings::PixelFormat::Yuv420p:
            return i18n::tr("capture_pixel_format_Yuv420p");
        case CaptureSettings::PixelFormat::Yuv444p:
            return i18n::tr("capture_pixel_format_Yuv444p");
        }
        return i18n::tr("capture_pixel_format_Yuv444p");
    }
}

void SettingsPanel::draw(float uiScale, Vec2i windowSize, Simulation& simulation, std::unique_ptr<IRenderer>& renderer,
                         CaptureController& captureController, FileDialogManager& fileDialog) {
    float target = visible ? 1.f : 0.f;
    float step = ImGui::GetIO().DeltaTime * 12.f;
    animProgress += (target - animProgress) * std::min(step, 1.f);

    if (animProgress < 0.01f) {
        return;
    }

    const float panelWidth = 300.f * uiScale;
    const float topOffset = 65.f * uiScale;
    const float panelHeight = static_cast<float>(windowSize.y) - topOffset;

    const float x = -panelWidth + panelWidth * animProgress;

    ImGui::SetNextWindowPos(ImVec2(x, topOffset));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::Begin("##SettingsPanel", nullptr, PANEL_FLAGS);

    ImGui::SeparatorText(i18n::tr("imgui_simulation").data());

    ImGui::TextUnformatted(i18n::tr("imgui_gravity").data());
    ImGui::SameLine();
    Vec3f gravity = simulation.getGravity();
    if (ImGui::Button(i18n::tr("imgui_reset_gravity").data(), ImVec2(50.f * uiScale, 0.f))) {
        simulation.setGravity(Vec3f(0, 0, 0));
        gravity = simulation.getGravity();
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
    gravityChanged |= drawDragFloat(i18n::tr("imgui_gravity_x").data(), gx, 0.05f, "%.2f");
    gravityChanged |= drawDragFloat(i18n::tr("imgui_gravity_y").data(), gy, 0.05f, "%.2f");
    gravityChanged |= drawDragFloat(i18n::tr("imgui_gravity_z").data(), gz, 0.05f, "%.2f");
    ImGui::PopItemWidth();
    if (gravityChanged) {
        simulation.setGravity(Vec3f(gx, gy, gz));
    }

    Integrator::Scheme currentIntegrator = simulation.getIntegrator();
    if (ComboStyle::beginCombo(i18n::tr("imgui_integrator").data(), integratorName(currentIntegrator).data(), 0.0f, uiScale)) {
        const Integrator::Scheme schemes[] = {
            Integrator::Scheme::Verlet,
            Integrator::Scheme::KDK,
            Integrator::Scheme::RK4,
            Integrator::Scheme::Langevin,
        };

        for (Integrator::Scheme scheme : schemes) {
            const bool isSelected = (scheme == currentIntegrator);
            if (ImGui::Selectable(integratorName(scheme).data(), isSelected)) {
                simulation.setIntegrator(scheme);
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
        ImGui::TextWrapped(i18n::tr("imgui_warning_not_implemented_used_as_velocity_verlet").data(),
                           integratorName(currentIntegrator).data());
        ImGui::PopStyleColor();
    }

    float maxParticleSpeed = simulation.getMaxParticleSpeed();
    ImGui::PushItemWidth(150.0f * uiScale);
    if (ImGui::SliderFloat(i18n::tr("imgui_speed_of_light").data(), &maxParticleSpeed, 0.0f, 100.0f,
                           maxParticleSpeed <= 0.0f ? i18n::tr("imgui_speed_of_light_unlimited").data() : "%.2f")) {
        simulation.setMaxParticleSpeed(maxParticleSpeed);
    }
    ImGui::PopItemWidth();

    float accelDamping = simulation.getAccelDamping();
    ImGui::PushItemWidth(150.0f * uiScale);
    if (ImGui::SliderFloat(i18n::tr("imgui_accel_damping").data(), &accelDamping, 0.0f, 1.0f, "%.3f")) {
        simulation.setAccelDamping(accelDamping);
    }
    ImGui::PopItemWidth();

    float dt = simulation.getDt();
    ImGui::PushItemWidth(150.0f * uiScale);
    if (ImGui::SliderFloat(i18n::tr("imgui_time_step").data(), &dt, 0.0001f, 0.05f, "%.4f",
                           ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic)) {
        simulation.setDt(dt);
    }
    ImGui::PopItemWidth();

    ImGui::SeparatorText(i18n::tr("imgui_worlds").data());
    const std::string activeWorldLabel = std::string(i18n::tr("imgui_world_prefix")) + std::to_string(simulation.activeWorldId());
    if (ComboStyle::beginCombo("##active_world", activeWorldLabel.c_str(), 180.0f * uiScale, uiScale, ImGuiComboFlags_HeightLargest)) {
        for (Simulation::WorldId worldId = 0; worldId < simulation.worldCount(); ++worldId) {
            const std::string worldLabel = std::string(i18n::tr("imgui_world_prefix")) + std::to_string(worldId);
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
    if (ImGui::Button(i18n::tr("imgui_create_world").data(), ImVec2(150.0f * uiScale, 0.0f))) {
        constexpr float worldGap = 20.0f;
        const Vec3f newWorldSize = simulation.world().getWorldSize();
        Vec3f newWorldOffset = simulation.world().getRenderOffset();
        float rightEdge = newWorldOffset.x + newWorldSize.x;
        for (Simulation::WorldId worldId = 0; worldId < simulation.worldCount(); ++worldId) {
            const World& world = simulation.worldAt(worldId);
            rightEdge = std::max(rightEdge, static_cast<float>(world.getRenderOffset().x + world.getWorldSize().x));
        }
        newWorldOffset.x = rightEdge + worldGap;
        const Simulation::WorldId newWorldId = simulation.createWorld(newWorldSize, newWorldOffset);
        simulation.setActiveWorld(newWorldId);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(simulation.worldCount() <= 1);
    if (ImGui::Button(i18n::tr("imgui_delete_world").data(), ImVec2(90.0f * uiScale, 0.0f))) {
        simulation.removeWorld(simulation.activeWorldId());
    }
    ImGui::EndDisabled();

    Vec3f renderOffset = simulation.world().getRenderOffset();
    ImGui::PushItemWidth(150.0f * uiScale);
    bool offsetChanged = false;
    offsetChanged |= drawDragFloat(i18n::tr("imgui_offset_x").data(), renderOffset.x, 0.25f, "%.1f");
    offsetChanged |= drawDragFloat(i18n::tr("imgui_offset_y").data(), renderOffset.y, 0.25f, "%.1f");
    offsetChanged |= drawDragFloat(i18n::tr("imgui_offset_z").data(), renderOffset.z, 0.25f, "%.1f");
    if (offsetChanged) {
        simulation.world().setRenderOffset(renderOffset);
    }
    ImGui::PopItemWidth();

    bool bondFormationEnabled = simulation.isBondFormationEnabled();
    if (ImGui::Checkbox(i18n::tr("imgui_bond_formation").data(), &bondFormationEnabled)) {
        simulation.setBondFormationEnabled(bondFormationEnabled);
    }

    bool ljEnabled = simulation.isLJEnabled();
    if (ImGui::Checkbox(i18n::tr("imgui_lj").data(), &ljEnabled)) {
        simulation.setLJEnabled(ljEnabled);
    }
    ImGui::SameLine();
    bool coulombEnabled = simulation.isCoulombEnabled();
    if (ImGui::Checkbox(i18n::tr("imgui_coulomb").data(), &coulombEnabled)) {
        simulation.setCoulombEnabled(coulombEnabled);
    }

    ImGui::SeparatorText(i18n::tr("imgui_render").data());
    ImGui::Checkbox(i18n::tr("imgui_grid").data(), &renderer->drawGrid);
    ImGui::Checkbox(i18n::tr("imgui_connections").data(), &renderer->drawBonds);
    ImGui::Checkbox(i18n::tr("imgui_box").data(), &renderer->drawBox);

    ImGui::TextUnformatted(i18n::tr("imgui_color_scheme").data());
    IRenderer::SpeedColorMode speedMode = renderer->speedColorMode;
    if (ComboStyle::beginCombo(i18n::tr("imgui_speed_color_mode").data(), speedColorModeName(speedMode).data(), 220.0f * uiScale, uiScale,
                               ImGuiComboFlags_HeightLargest)) {
        const IRenderer::SpeedColorMode modes[] = {
            IRenderer::SpeedColorMode::AtomColor,
            IRenderer::SpeedColorMode::GradientClassic,
            IRenderer::SpeedColorMode::GradientTurbo,
        };

        for (IRenderer::SpeedColorMode mode : modes) {
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

    renderer->speedColorMode = speedMode;

    ImGui::TextUnformatted(i18n::tr("imgui_max_gradien_velocity").data());

    static float manualSpeedGradientMax = 5.0f;
    bool autoSpeedGradient = renderer->speedGradientMax <= 0.0f;
    const bool gradientModeEnabled = renderer->speedColorMode != IRenderer::SpeedColorMode::AtomColor;
    if (!autoSpeedGradient) {
        manualSpeedGradientMax = renderer->speedGradientMax;
    }

    ImGui::PushItemWidth(180.0f * uiScale);
    ImGui::BeginDisabled(autoSpeedGradient || !gradientModeEnabled);
    if (ImGui::SliderFloat(i18n::tr("imgui_speed_gradient_max_slider").data(), &manualSpeedGradientMax, 0.1f, 10.0f, "%.2f")) {
        renderer->speedGradientMax = manualSpeedGradientMax;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!gradientModeEnabled);
    if (ImGui::Checkbox(i18n::tr("imgui_auto_speed_gradien").data(), &autoSpeedGradient)) {
        renderer->speedGradientMax = autoSpeedGradient ? 0.0f : manualSpeedGradientMax;
    }
    ImGui::EndDisabled();
    ImGui::PopItemWidth();

    ImGui::SeparatorText(i18n::tr("imgui_neighbour_list").data());
    int cellSize = simulation.world().getGridCellSize();
    if (ImGui::SliderInt(i18n::tr("imgui_cell_size").data(), &cellSize, 1, 32)) {
        simulation.setSizeBox(simulation.world().getWorldSize(), cellSize);
    }

    float cutoff = simulation.getNeighborListCutoff();
    if (ImGui::SliderFloat(i18n::tr("imgui_cutoff_nl").data(), &cutoff, 0.5f, 20.0f, "%.2f")) {
        simulation.setNeighborListCutoff(cutoff);
    }

    float skin = simulation.getNeighborListSkin();
    if (ImGui::SliderFloat(i18n::tr("imgui_skin_nl").data(), &skin, 0.1f, 10.0f, "%.2f")) {
        simulation.setNeighborListSkin(skin);
    }

    if (captureController.isAvailable()) {
        ImGui::SeparatorText(i18n::tr("imgui_write").data());
        CaptureSettings captureSettings = captureController.settings();
        const bool recordingActive = captureController.isRecording();
        const std::string captureDir = captureController.outputDirectory().string();

        ImGui::TextUnformatted(i18n::tr("imgui_video_saving_folder").data());
        std::array<char, 512> captureDirBuffer{};
        std::snprintf(captureDirBuffer.data(), captureDirBuffer.size(), "%s", captureDir.data());
        const float browseButtonWidth = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseButtonWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputText(i18n::tr("imgui_capture_dir").data(), captureDirBuffer.data(), captureDirBuffer.size(),
                         ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button(i18n::tr("imgui_capture_dir_browse").data(), ImVec2(browseButtonWidth, 0.0f))) {
            fileDialog.openCaptureDirectory(captureDir);
        }

        ImGui::BeginDisabled(recordingActive);

        if (ImGui::BeginTable(i18n::tr("imgui_capture_settings_table").data(), 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-FLT_MIN);
            int captureFps = captureSettings.fps;
            if (ImGui::SliderInt(i18n::tr("imgui_fps_capture").data(), &captureFps, 10, 60)) {
                captureSettings.fps = captureFps;
                captureController.setSettings(captureSettings);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            int crf = captureSettings.crf;
            if (ImGui::SliderInt(i18n::tr("imgui_crf_capture").data(), &crf, 12, 30)) {
                captureSettings.crf = crf;
                captureController.setSettings(captureSettings);
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            CaptureSettings::Preset preset = captureSettings.preset;
            if (ComboStyle::beginCombo(i18n::tr("imgui_preset_capture").data(), capturePresetName(preset).data(), -FLT_MIN, uiScale)) {
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
            if (ComboStyle::beginCombo(i18n::tr("imgui_color_capture").data(), capturePixelFormatName(pixelFormat).data(), -FLT_MIN,
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

    if (ImGui::Button(i18n::tr("imgui_reset_settings").data(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        const UserSettings defaults;
        captureController.setOutputDirectory(defaults.captureOutputDirectory);
        captureController.setSettings(defaults.captureSettings);

        renderer->drawGrid = defaults.rendererDrawGrid;
        renderer->drawBonds = defaults.rendererDrawBonds;
        renderer->drawBox = defaults.rendererDrawBox;
        renderer->speedColorMode = defaults.rendererSpeedColorMode;
        renderer->speedGradientMax = defaults.rendererSpeedGradientMax;

        simulation.setIntegrator(defaults.simulationIntegrator);
        simulation.setBondFormationEnabled(defaults.simulationBondFormationEnabled);
        simulation.setLJEnabled(defaults.simulationLJEnabled);
        simulation.setCoulombEnabled(defaults.simulationCoulombEnabled);
    }

    ImGui::SeparatorText(i18n::tr("imgui_localization").data());
    const float languageButtonWidth = 86.0f * uiScale;
    if (ImGui::Button(i18n::tr("imgui_language_toggle").data(), ImVec2(languageButtonWidth, 0.0f))) {
        i18n::toggle();
    }

    const float exitButtonWidth = ImGui::GetContentRegionAvail().x;
    const std::string versionText =
        std::string(i18n::tr("version_text_pre")) + LATTICELAB_VERSION_STRING + std::string(i18n::tr("version_text_after"));
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

    if (ImGui::Button(i18n::tr("imgui_exit_button").data(), ImVec2(exitButtonWidth, 0.0f))) {
        AppSignals::UI::ExitApplication.emit();
    }

    ImGui::End();
}
