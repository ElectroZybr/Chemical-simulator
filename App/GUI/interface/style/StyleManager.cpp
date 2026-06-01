#include "StyleManager.h"

#include <algorithm>

void StyleManager::applyCustomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowPadding = ImVec2(7.5, 7.5);
    style.WindowRounding = 7.5;
    style.FramePadding = ImVec2(2.5, 2.5);
    style.ItemSpacing = ImVec2(6, 4);
    style.ItemInnerSpacing = ImVec2(4, 3);
    style.IndentSpacing = 12.5;
    style.ScrollbarSize = 7.5;
    style.ScrollbarRounding = 7.5;
    style.GrabMinSize = 15;
    style.GrabRounding = 3.5;
    style.FrameRounding = 5;

    colors[ImGuiCol_Text] = ImVec4(0.95, 0.96, 0.98, 1.00);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.36, 0.42, 0.47, 1.00);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11, 0.15, 0.17, 1.00);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15, 0.18, 0.22, 1.00);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08, 0.08, 0.08, 0.94);
    colors[ImGuiCol_Border] = ImVec4(0.43, 0.43, 0.50, 0.50);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00, 0.00, 0.00, 0.00);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20, 0.25, 0.29, 1.00);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12, 0.20, 0.28, 1.00);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.09, 0.12, 0.14, 1.00);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09, 0.12, 0.14, 0.65);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.00, 0.00, 0.00, 0.51);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08, 0.10, 0.12, 1.00);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15, 0.18, 0.22, 1.00);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02, 0.02, 0.02, 0.39);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20, 0.25, 0.29, 1.00);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18, 0.22, 0.25, 1.00);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09, 0.21, 0.31, 1.00);
    colors[ImGuiCol_CheckMark] = ImVec4(0.28, 0.56, 1.00, 1.00);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28, 0.56, 1.00, 1.00);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37, 0.61, 1.00, 1.00);
    colors[ImGuiCol_Button] = ImVec4(0.20, 0.25, 0.29, 1.00);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18, 0.23, 0.25, 1.00);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06, 0.53, 0.98, 1.00);
    colors[ImGuiCol_Header] = ImVec4(0.20, 0.25, 0.29, 0.55);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26, 0.59, 0.98, 0.80);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26, 0.59, 0.98, 1.00);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26, 0.59, 0.98, 0.25);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26, 0.59, 0.98, 0.67);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06, 0.05, 0.07, 1.00);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61, 0.61, 0.61, 1.00);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00, 0.43, 0.35, 1.00);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90, 0.70, 0.00, 1.00);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00, 0.60, 0.00, 1.00);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25, 1.00, 0.00, 0.43);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00, 0.98, 0.95, 0.73);

    baseStyle = style;
    scale = 1;
    ImGui::GetIO().FontGlobalScale = 1.0f;
}

void StyleManager::onResize(Vec2i newSize) {
    Vec2f s = Vec2f(newSize) / Vec2f(BASE_W, BASE_H);
    scale = std::clamp(std::min(s.x, s.y), 0.85f, 1.35f);

    ImGui::GetStyle() = baseStyle;
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetIO().FontGlobalScale = 1.0f;
}
