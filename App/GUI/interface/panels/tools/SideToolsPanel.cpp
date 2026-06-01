#include "SideToolsPanel.h"

#include <array>
#include <cmath>

#define ICON_FA_MOUSE_POINTER "\uf245"
#define ICON_FA_VECTOR_SQUARE "\uf5cb"
#define ICON_FA_DRAW_POLYGON "\uf5ee"
#define ICON_FA_RULER "\uf545"
#define ICON_FA_PLUS "\uf067"
#define ICON_FA_MINUS "\uf068"

namespace {
    constexpr ImVec4 ACTIVE_COLOR = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

    struct ToolItem {
        SideToolsPanel::Tool tool;
        const char* icon;
        const char* tooltip;
    };

    constexpr std::array<ToolItem, 6> TOOL_ITEMS{{
        {SideToolsPanel::Tool::Cursor, ICON_FA_MOUSE_POINTER, "Cursor"},
        {SideToolsPanel::Tool::Frame, ICON_FA_VECTOR_SQUARE, "Frame select"},
        {SideToolsPanel::Tool::Lasso, ICON_FA_DRAW_POLYGON, "Lasso select"},
        {SideToolsPanel::Tool::Ruler, ICON_FA_RULER, "Ruler"},
        {SideToolsPanel::Tool::AddAtom, ICON_FA_PLUS, "Add atom"},
        {SideToolsPanel::Tool::RemoveAtom, ICON_FA_MINUS, "Remove atom"},
    }};

    bool drawToolButton(const char* icon, const char* tooltip, bool selected, float buttonSize, ImFont* textFont) {
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ACTIVE_COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACTIVE_COLOR);
        }

        const bool pressed = ImGui::Button(icon, ImVec2(buttonSize, buttonSize));

        if (selected) {
            ImGui::PopStyleColor(3);
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            if (textFont) {
                ImGui::PushFont(textFont);
            }
            ImGui::TextUnformatted(tooltip);
            if (textFont) {
                ImGui::PopFont();
            }
            ImGui::EndTooltip();
        }

        return pressed;
    }
}

void SideToolsPanel::draw(float scale, Vec2i windowSize, ImFont* iconFont, ImFont* textFont) {
    constexpr float baseTopOffset = 114.0f;
    constexpr float baseRightOffset = 0.0f;
    constexpr float baseSpacing = 4.0f;
    constexpr float baseButtonSize = 44.0f;
    constexpr float basePaddingX = 6.0f;
    constexpr float basePaddingY = 6.0f;
    const float buttonCount = static_cast<float>(TOOL_ITEMS.size());

    const float buttonSize = std::round(baseButtonSize * scale);
    const float spacingY = std::round(baseSpacing * scale);
    const float panelPaddingX = std::round(basePaddingX * scale);
    const float panelPaddingY = std::round(basePaddingY * scale);
    const float rightOffset = std::round(baseRightOffset * scale);

    const float panelWidthRaw = (panelPaddingX * 2.0f) + buttonSize;
    const float panelHeightRaw = (panelPaddingY * 2.0f) + (buttonCount * buttonSize) + ((buttonCount - 1.0f) * spacingY);
    const float xRaw = static_cast<float>(windowSize.x) - panelWidthRaw - rightOffset;
    const float yRaw = baseTopOffset * scale;

    const float panelWidth = std::round(panelWidthRaw);
    const float panelHeight = std::round(panelHeightRaw);
    const float x = std::round(xRaw);
    const float y = std::round(yRaw);

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(panelPaddingX, panelPaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, spacingY));
    ImGui::Begin("SideTools", nullptr, PANEL_FLAGS);

    if (iconFont) {
        ImGui::PushFont(iconFont);
    }

    for (const ToolItem& item : TOOL_ITEMS) {
        if (drawToolButton(item.icon, item.tooltip, selectedTool == item.tool, buttonSize, textFont)) {
            selectedTool = item.tool;
        }
    }

    if (iconFont) {
        ImGui::PopFont();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}
