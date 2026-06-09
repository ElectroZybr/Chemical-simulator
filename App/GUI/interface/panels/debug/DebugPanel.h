#pragma once
#include <deque>

#include "GUI/interface/panels/debug/view/DebugView.h"

class DebugPanel {
    std::deque<DebugView> views;
    bool visible = false;
    float animProgress = 0.f;

public:
    DebugView* addView(DebugView view);

    void draw(float uiScale, glm::ivec2 windowSize);

    void toggle() { visible = !visible; }
    void close() { visible = false; }
    bool isVisible() const { return visible; }
};
