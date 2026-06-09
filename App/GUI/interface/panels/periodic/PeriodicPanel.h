#pragma once
#include <imgui.h>
#include <glm/vec2.hpp>

class PeriodicPanel {
public:
    static constexpr ImGuiWindowFlags PANEL_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    void draw(float scale, glm::ivec2 windowSize, int& selectedAtom);

    static int decodeAtom(int index);

private:
    float animProgress = 0.0f;
};
