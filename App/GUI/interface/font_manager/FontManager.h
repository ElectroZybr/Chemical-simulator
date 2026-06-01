#pragma once
#include <imgui.h>

class FontManager {
public:
    bool load(float uiScale = 1.0f);

    ImFont* main = nullptr;
    ImFont* dialog = nullptr;
    ImFont* icons = nullptr;
};
