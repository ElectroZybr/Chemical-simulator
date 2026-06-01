#include "SimControlPanel.h"

#include <algorithm>
#include <cmath>

#include "App/AppSignals.h"

#define ICON_FA_PAUSE "\uf04c"
#define ICON_FA_PLAY "\uf04b"
#define ICON_FA_STEP_FORWARD "\uf051"

static const ImVec4 ACTIVE_COLOR = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
static const ImVec4 DISABLED_BUTTON_COLOR = ImVec4(0.26f, 0.28f, 0.31f, 1.00f);
static const ImVec4 DISABLED_BUTTON_HOVERED_COLOR = ImVec4(0.26f, 0.28f, 0.31f, 1.00f);
static const ImVec4 DISABLED_BUTTON_ACTIVE_COLOR = ImVec4(0.26f, 0.28f, 0.31f, 1.00f);

static void pushActiveColor() {
    ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACTIVE_COLOR);
}

static float speedToSlider(float speed) {
    constexpr float kMinSpeed = 1.0f;
    constexpr float kMaxSpeed = 10000.0f;
    constexpr float kCurve = 2.2f;

    const float normalized = std::clamp((speed - kMinSpeed) / (kMaxSpeed - kMinSpeed), 0.0f, 1.0f);
    return std::pow(normalized, 1.0f / kCurve);
}

static float sliderToSpeed(float slider) {
    constexpr float kMinSpeed = 1.0f;
    constexpr float kMaxSpeed = 10000.0f;
    constexpr float kCurve = 2.2f;

    const float normalized = std::pow(std::clamp(slider, 0.0f, 1.0f), kCurve);
    return kMinSpeed + (kMaxSpeed - kMinSpeed) * normalized;
}

void SimControlPanel::draw(float scale, Vec2i windowSize, bool& pause, float& simulationSpeed, int simStep, float deltaTime) {
    sampleAccum_ += deltaTime;
    if (sampleAccum_ >= 0.25f) {
        const int deltaSteps = simStep - lastSimStepSample_;
        displayedStepsPerSecond_ = sampleAccum_ > 0.0f ? static_cast<float>(deltaSteps) / sampleAccum_ : 0.0f;
        lastSimStepSample_ = simStep;
        sampleAccum_ = 0.0f;
    }

    ImGui::SetNextWindowPos(ImVec2(windowSize.x - 122 * scale, 0));
    ImGui::SetNextWindowSize(ImVec2(122 * scale, 111 * scale));
    ImGui::Begin("SimControl", nullptr, PANEL_FLAGS);

    if (!pause) {
        ImGui::PushStyleColor(ImGuiCol_Button, DISABLED_BUTTON_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DISABLED_BUTTON_HOVERED_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DISABLED_BUTTON_ACTIVE_COLOR);
    }
    ImGui::BeginDisabled(!pause);
    if (ImGui::Button(ICON_FA_STEP_FORWARD, ImVec2(50 * scale, 50 * scale))) {
        AppSignals::UI::StepPhysics.emit();
    }
    ImGui::EndDisabled();
    if (!pause) {
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine();

    const bool playButtonHighlighted = pause;
    if (playButtonHighlighted) {
        pushActiveColor();
    }
    if (ImGui::Button(pause ? ICON_FA_PLAY : ICON_FA_PAUSE, ImVec2(50 * scale, 50 * scale))) {
        pause = !pause;
    }
    if (playButtonHighlighted) {
        ImGui::PopStyleColor(3);
    }

    float speedSlider = speedToSlider(simulationSpeed);
    ImGui::PushItemWidth(106 * scale);
    if (ImGui::SliderFloat("##Speed", &speedSlider, 0.0f, 1.0f, "", ImGuiSliderFlags_AlwaysClamp)) {
        simulationSpeed = sliderToSpeed(speedSlider);
    }
    const ImVec2 sliderMin = ImGui::GetItemRectMin();
    const ImVec2 sliderMax = ImGui::GetItemRectMax();
    ImGui::PopItemWidth();

    const char* valueText = pause ? "pause" : nullptr;
    char valueBuffer[32]{};
    if (valueText == nullptr) {
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.0f", displayedStepsPerSecond_);
        valueText = valueBuffer;
    }

    const ImVec2 textSize = ImGui::CalcTextSize(valueText);
    const ImVec2 textPos(sliderMin.x + (sliderMax.x - sliderMin.x - textSize.x) * 0.5f,
                         sliderMin.y + (sliderMax.y - sliderMin.y - textSize.y) * 0.5f);
    ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(235, 235, 235, 255), valueText);

    ImGui::End();
}
