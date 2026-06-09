#pragma once
#include <imgui.h>
#include <glm/vec2.hpp>

class SimControlPanel {
public:
    static constexpr ImGuiWindowFlags PANEL_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    void draw(float scale, glm::ivec2 windowSize, bool& pause, float& simulationSpeed, int simStep, float deltaTime);

private:
    float displayedStepsPerSecond_ = 0.0f;
    float sampleAccum_ = 0.0f;
    int lastSimStepSample_ = 0;
};
