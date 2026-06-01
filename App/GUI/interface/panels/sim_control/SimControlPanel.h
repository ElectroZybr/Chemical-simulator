#pragma once
#include <imgui.h>

#include "Lattice/Engine/math/Vec2.h"

class SimControlPanel {
public:
    static constexpr ImGuiWindowFlags PANEL_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    void draw(float scale, Vec2i windowSize, bool& pause, float& simulationSpeed, int simStep, float deltaTime);

private:
    float displayedStepsPerSecond_ = 0.0f;
    float sampleAccum_ = 0.0f;
    int lastSimStepSample_ = 0;
};
