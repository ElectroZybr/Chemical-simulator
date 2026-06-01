#pragma once
#include <imgui.h>

#include "Lattice/Engine/math/Vec2.h"

class StyleManager {
public:
    void applyCustomStyle();
    void onResize(Vec2i newSize);
    float getScale() const { return scale; }

private:
    ImGuiStyle baseStyle;
    float scale = 1.0f;

    // базовый размер окна
    static constexpr int BASE_W = 1920;
    static constexpr int BASE_H = 1080;
};
