#pragma once

#include <cstdint>

#include <imgui.h>

#include "Lattice/Engine/math/Vec2.h"

class SideToolsPanel {
public:
    enum class Tool : uint8_t { Cursor, Frame, Lasso, Ruler, AddAtom, RemoveAtom };

    static constexpr ImGuiWindowFlags PANEL_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    void draw(float scale, Vec2i windowSize, ImFont* iconFont, ImFont* textFont = nullptr);

    Tool getSelectedTool() const { return selectedTool; }
    void setSelectedTool(Tool tool) { selectedTool = tool; }

private:
    Tool selectedTool = Tool::Cursor;
};
