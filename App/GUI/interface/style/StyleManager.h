#pragma once
#include <imgui.h>
#include <glm/vec2.hpp>

class StyleManager {
public:
    static constexpr float kMinUiScale = 0.60f;
    static constexpr float kMaxUiScale = 1.50f;
    static constexpr float kDefaultUiScale = 1.00f;

    void applyCustomStyle();
    void onResize(glm::ivec2 newSize);
    void setUserScaleMultiplier(float multiplier);
    float getScale() const { return scale; }
    float getUserScaleMultiplier() const { return userScaleMultiplier; }

private:
    void refreshScale();

    ImGuiStyle baseStyle;
    float adaptiveScale = 1.0f;
    float userScaleMultiplier = 1.0f;
    float scale = 1.0f;

    // базовый размер окна
    static constexpr int BASE_W = 1920;
    static constexpr int BASE_H = 1080;
};
