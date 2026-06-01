#include "ToolsPanel.h"

#include <cmath>

#include "App/AppSignals.h"
#include "GUI/interface/panels/debug/DebugPanel.h"
#include "GUI/interface/panels/io/ioPanel.h"
#include "GUI/interface/panels/settings/SettingsPanel.h"
#include "Rendering/camera/Camera.h"

#define ICON_FA_FLASK "\uf0c3"
#define ICON_FA_COG "\uf013"
#define ICON_FA_BUG "\uf188"
#define ICON_FA_SYNC_ALT "\uf2f1"
#define ICON_FA_STREET_VIEW "\uf21d"

void ToolsPanel::setRendererType(RendererType type) {
    is3D = type == RendererType::Renderer3D;
    if (!is3D) {
        isFree = false;
    }
}

void ToolsPanel::draw(float scale, DebugPanel& debug, SettingsPanel& settings, IOPanel& ioPanel) {
    constexpr float baseTopOffset = 0.0f;
    constexpr float baseLeftOffset = 0.0f;
    constexpr float baseSpacing = 4.0f;
    constexpr float baseButtonSize = 50.0f;
    constexpr float basePaddingX = 6.0f;
    constexpr float basePaddingY = 6.0f;

    const float buttonCount = 4.0f + (is3D ? 1.f : 0.f);
    const float buttonSize = std::round(baseButtonSize * scale);
    const float spacingX = std::round(baseSpacing * scale);
    const float panelPaddingX = std::round(basePaddingX * scale);
    const float panelPaddingY = std::round(basePaddingY * scale);

    const float panelWidth = std::round((panelPaddingX * 2.0f) + (buttonCount * buttonSize) + ((buttonCount - 1.0f) * spacingX));
    const float panelHeight = std::round((panelPaddingY * 2.0f) + buttonSize);
    const float x = std::round(baseLeftOffset * scale);
    const float y = std::round(baseTopOffset * scale);

    auto drawActiveButton = [&](const char* icon, bool visible) {
        if (visible) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
        }
        const bool clicked = ImGui::Button(icon, ImVec2(buttonSize, buttonSize));
        if (visible) {
            ImGui::PopStyleColor(3);
        }
        return clicked;
    };

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(panelPaddingX, panelPaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacingX, 0.0f));
    ImGui::Begin("Tools", nullptr, PANEL_FLAGS);

    if (drawActiveButton(ICON_FA_COG, settings.isVisible())) {
        if (settings.isVisible()) {
            settings.close();
        }
        else {
            settings.toggle();
            debug.close();
            ioPanel.close();
        }
    }

    ImGui::SameLine();
    if (drawActiveButton(ICON_FA_FLASK, ioPanel.isVisible())) {
        if (ioPanel.isVisible()) {
            ioPanel.close();
        }
        else {
            ioPanel.toggle();
            settings.close();
            debug.close();
        }
    }
    ImGui::SameLine();

    if (drawActiveButton(ICON_FA_BUG, debug.isVisible())) {
        if (debug.isVisible()) {
            debug.close();
        }
        else {
            debug.toggle();
            settings.close();
            ioPanel.close();
        }
    }
    ImGui::SameLine();

    if (ImGui::Button(is3D ? "3D" : "2D", ImVec2(buttonSize, buttonSize))) {
        is3D = !is3D;
        if (!is3D) {
            isFree = false;
        }
        AppSignals::UI::SetRender.emit(is3D ? RendererType::Renderer3D : RendererType::Renderer2D);
    }
    if (is3D) {
        ImGui::SameLine();
        if (ImGui::Button(isFree ? ICON_FA_STREET_VIEW : ICON_FA_SYNC_ALT, ImVec2(buttonSize, buttonSize))) {
            isFree = !isFree;
            AppSignals::UI::SetCameraMode.emit(isFree ? Camera::Mode::Free : Camera::Mode::Orbit);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}
