#include "SettingsPanel.h"

#include <array>
#include <cstdio>

#include <imgui.h>

#include "App/AppSignals.h"
#include "App/UserSettings.h"
#include "App/capture/CaptureController.h"
#include "Engine/Simulation.h"
#include "GUI/interface/file_dialog/FileDialogManager.h"
#include "GUI/interface/style/ComboStyle.h"
#include "Rendering/BaseRenderer.h"
#include "generated/AppVersion.h"

namespace {
    const char* integratorName(Integrator::Scheme scheme) {
        switch (scheme) {
        case Integrator::Scheme::Verlet:
            return "Velocity Verlet";
        case Integrator::Scheme::KDK:
            return "KDK";
        case Integrator::Scheme::RK4:
            return "Runge-Kutta 4";
        case Integrator::Scheme::Langevin:
            return "Langevin";
        }
        return "Unknown";
    }

    const char* speedColorModeName(IRenderer::SpeedColorMode mode) {
        switch (mode) {
        case IRenderer::SpeedColorMode::AtomColor:
            return "Обычная раскраска";
        case IRenderer::SpeedColorMode::GradientClassic:
            return "Градиент скорости";
        case IRenderer::SpeedColorMode::GradientTurbo:
            return "Турбо градиент";
        }
        return "Обычная раскраска";
    }

    const char* capturePresetName(CaptureSettings::Preset preset) {
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

    const char* capturePixelFormatName(CaptureSettings::PixelFormat pixelFormat) {
        switch (pixelFormat) {
        case CaptureSettings::PixelFormat::Yuv420p:
            return "I420";
        case CaptureSettings::PixelFormat::Yuv444p:
            return "I444";
        }
        return "I444";
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

    ImGui::SeparatorText("Симуляция");

    ImGui::TextUnformatted("Гравитация");
    ImGui::SameLine();
    Vec3f gravity = simulation.getGravity();
    if (ImGui::Button("Reset##gravity", ImVec2(50.f * uiScale, 0.f))) {
        simulation.setGravity(Vec3f(0, 0, 0));
        gravity = simulation.getGravity();
    }

    float gx = gravity.x;
    float gy = gravity.y;
    float gz = gravity.z;
    bool gravityChanged = false;
    gravityChanged |= ImGui::SliderFloat("Gravity X##gravity_x", &gx, -10.0f, 10.0f, "%.2f");
    gravityChanged |= ImGui::SliderFloat("Gravity Y##gravity_y", &gy, -10.0f, 10.0f, "%.2f");
    gravityChanged |= ImGui::SliderFloat("Gravity Z##gravity_z", &gz, -10.0f, 10.0f, "%.2f");
    if (gravityChanged) {
        simulation.setGravity(Vec3f(gx, gy, gz));
    }

    Integrator::Scheme currentIntegrator = simulation.getIntegrator();
    if (ComboStyle::beginCombo("Integrator", integratorName(currentIntegrator), 0.0f, uiScale)) {
        const Integrator::Scheme schemes[] = {
            Integrator::Scheme::Verlet,
            Integrator::Scheme::KDK,
            Integrator::Scheme::RK4,
            Integrator::Scheme::Langevin,
        };

        for (Integrator::Scheme scheme : schemes) {
            const bool isSelected = (scheme == currentIntegrator);
            if (ImGui::Selectable(integratorName(scheme), isSelected)) {
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
        ImGui::TextWrapped("Внимание: %s пока не реализован и временно работает как Velocity Verlet.", integratorName(currentIntegrator));
        ImGui::PopStyleColor();
    }

    float maxParticleSpeed = simulation.getMaxParticleSpeed();
    ImGui::PushItemWidth(150.0f * uiScale);
    if (ImGui::SliderFloat("Скорость света", &maxParticleSpeed, 0.0f, 100.0f, maxParticleSpeed <= 0.0f ? "не ограничена" : "%.2f")) {
        simulation.setMaxParticleSpeed(maxParticleSpeed);
    }
    ImGui::PopItemWidth();

    float accelDamping = simulation.getAccelDamping();
    ImGui::PushItemWidth(150.0f * uiScale);
    if (ImGui::SliderFloat("Accel damping", &accelDamping, 0.0f, 1.0f, "%.3f")) {
        simulation.setAccelDamping(accelDamping);
    }
    ImGui::PopItemWidth();

    float dt = simulation.getDt();
    ImGui::PushItemWidth(150.0f * uiScale);
    if (ImGui::SliderFloat("Time step (Dt)", &dt, 0.0001f, 0.05f, "%.4f", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic)) {
        simulation.setDt(dt);
    }
    ImGui::PopItemWidth();

    bool bondFormationEnabled = simulation.isBondFormationEnabled();
    if (ImGui::Checkbox("Образовывать связи", &bondFormationEnabled)) {
        simulation.setBondFormationEnabled(bondFormationEnabled);
    }

    bool ljEnabled = simulation.isLJEnabled();
    if (ImGui::Checkbox("LJ", &ljEnabled)) {
        simulation.setLJEnabled(ljEnabled);
    }
    ImGui::SameLine();
    bool coulombEnabled = simulation.isCoulombEnabled();
    if (ImGui::Checkbox("Coulomb", &coulombEnabled)) {
        simulation.setCoulombEnabled(coulombEnabled);
    }

    ImGui::SeparatorText("Рендер");
    ImGui::Checkbox("Сетка", &renderer->drawGrid);
    ImGui::Checkbox("Связи", &renderer->drawBonds);

    ImGui::TextUnformatted("Цветовая схема");
    IRenderer::SpeedColorMode speedMode = renderer->speedColorMode;
    if (ComboStyle::beginCombo("##speed_color_mode", speedColorModeName(speedMode), 220.0f * uiScale, uiScale,
                               ImGuiComboFlags_HeightLargest)) {
        const IRenderer::SpeedColorMode modes[] = {
            IRenderer::SpeedColorMode::AtomColor,
            IRenderer::SpeedColorMode::GradientClassic,
            IRenderer::SpeedColorMode::GradientTurbo,
        };

        for (IRenderer::SpeedColorMode mode : modes) {
            const bool isSelected = (mode == speedMode);
            if (ImGui::Selectable(speedColorModeName(mode), isSelected)) {
                speedMode = mode;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    renderer->speedColorMode = speedMode;

    ImGui::TextUnformatted("Макс. скорость градиента");

    static float manualSpeedGradientMax = 5.0f;
    bool autoSpeedGradient = renderer->speedGradientMax <= 0.0f;
    const bool gradientModeEnabled = renderer->speedColorMode != IRenderer::SpeedColorMode::AtomColor;
    if (!autoSpeedGradient) {
        manualSpeedGradientMax = renderer->speedGradientMax;
    }

    ImGui::PushItemWidth(180.0f * uiScale);
    ImGui::BeginDisabled(autoSpeedGradient || !gradientModeEnabled);
    if (ImGui::SliderFloat("##speed_gradient_max", &manualSpeedGradientMax, 0.1f, 10.0f, "%.2f")) {
        renderer->speedGradientMax = manualSpeedGradientMax;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!gradientModeEnabled);
    if (ImGui::Checkbox("Авто##speed_gradient_auto", &autoSpeedGradient)) {
        renderer->speedGradientMax = autoSpeedGradient ? 0.0f : manualSpeedGradientMax;
    }
    ImGui::EndDisabled();
    ImGui::PopItemWidth();

    ImGui::SeparatorText("Список соседей");
    int cellSize = simulation.world().getGridCellSize();
    if (ImGui::SliderInt("Cell size", &cellSize, 1, 32)) {
        simulation.setSizeBox(simulation.world().getWorldSize(), cellSize);
    }

    float cutoff = simulation.getNeighborListCutoff();
    if (ImGui::SliderFloat("Cutoff NL", &cutoff, 0.5f, 20.0f, "%.2f")) {
        simulation.setNeighborListCutoff(cutoff);
    }

    float skin = simulation.getNeighborListSkin();
    if (ImGui::SliderFloat("Skin NL", &skin, 0.1f, 10.0f, "%.2f")) {
        simulation.setNeighborListSkin(skin);
    }

    if (captureController.isAvailable()) {
        ImGui::SeparatorText("Запись");
        CaptureSettings captureSettings = captureController.settings();
        const bool recordingActive = captureController.isRecording();
        const std::string captureDir = captureController.outputDirectory().string();

        ImGui::TextUnformatted("Папка сохранения видео");
        std::array<char, 512> captureDirBuffer{};
        std::snprintf(captureDirBuffer.data(), captureDirBuffer.size(), "%s", captureDir.data());
        const float browseButtonWidth = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseButtonWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputText("##capture_dir", captureDirBuffer.data(), captureDirBuffer.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("...##capture_dir_browse", ImVec2(browseButtonWidth, 0.0f))) {
            fileDialog.openCaptureDirectory(captureDir);
        }

        ImGui::BeginDisabled(recordingActive);

        if (ImGui::BeginTable("##capture_settings_table", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-FLT_MIN);
            int captureFps = captureSettings.fps;
            if (ImGui::SliderInt("FPS##capture_fps", &captureFps, 10, 60)) {
                captureSettings.fps = captureFps;
                captureController.setSettings(captureSettings);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            int crf = captureSettings.crf;
            if (ImGui::SliderInt("CRF##capture_crf", &crf, 12, 30)) {
                captureSettings.crf = crf;
                captureController.setSettings(captureSettings);
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            CaptureSettings::Preset preset = captureSettings.preset;
            if (ComboStyle::beginCombo("Preset##capture_preset", capturePresetName(preset), -FLT_MIN, uiScale)) {
                const CaptureSettings::Preset presets[] = {
                    CaptureSettings::Preset::Ultrafast, CaptureSettings::Preset::Veryfast, CaptureSettings::Preset::Faster,
                    CaptureSettings::Preset::Fast,      CaptureSettings::Preset::Medium,
                };

                for (CaptureSettings::Preset candidate : presets) {
                    const bool isSelected = (candidate == preset);
                    if (ImGui::Selectable(capturePresetName(candidate), isSelected)) {
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
            if (ComboStyle::beginCombo("Цвет##capture_color", capturePixelFormatName(pixelFormat), -FLT_MIN, uiScale)) {
                const CaptureSettings::PixelFormat pixelFormats[] = {
                    CaptureSettings::PixelFormat::Yuv444p,
                    CaptureSettings::PixelFormat::Yuv420p,
                };

                for (CaptureSettings::PixelFormat candidate : pixelFormats) {
                    const bool isSelected = (candidate == pixelFormat);
                    if (ImGui::Selectable(capturePixelFormatName(candidate), isSelected)) {
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

    if (ImGui::Button("Reset settings", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        const UserSettings defaults;
        captureController.setOutputDirectory(defaults.captureOutputDirectory);
        captureController.setSettings(defaults.captureSettings);

        renderer->drawGrid = defaults.rendererDrawGrid;
        renderer->drawBonds = defaults.rendererDrawBonds;
        renderer->speedColorMode = defaults.rendererSpeedColorMode;
        renderer->speedGradientMax = defaults.rendererSpeedGradientMax;

        simulation.setIntegrator(defaults.simulationIntegrator);
        simulation.setBondFormationEnabled(defaults.simulationBondFormationEnabled);
        simulation.setLJEnabled(defaults.simulationLJEnabled);
        simulation.setCoulombEnabled(defaults.simulationCoulombEnabled);
    }

    const float exitButtonWidth = ImGui::GetContentRegionAvail().x;
    const char* versionText = "LatticeLab v" LATTICELAB_VERSION_STRING " demo";
    const float versionWidth = ImGui::CalcTextSize(versionText).x;
    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing();
    const float remaining = ImGui::GetContentRegionAvail().y - footerHeight;
    if (remaining > 0.0f) {
        ImGui::Dummy(ImVec2(0.0f, remaining));
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.76f, 0.80f, 0.85f));
    ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - versionWidth) * 0.5f));
    ImGui::TextUnformatted(versionText);
    ImGui::PopStyleColor();

    if (ImGui::Button("Exit", ImVec2(exitButtonWidth, 0.0f))) {
        AppSignals::UI::ExitApplication.emit();
    }

    ImGui::End();
}
