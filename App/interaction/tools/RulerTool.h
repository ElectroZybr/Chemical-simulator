#pragma once

#include "App/interaction/tools/ITool.h"

class RulerTool final : public ITool {
public:
    explicit RulerTool(ToolContext& context) noexcept;

    void onLeftPressed(glm::ivec2 mousePos) override;
    void onLeftReleased(glm::ivec2 mousePos) override;
    bool onRightPressed(glm::ivec2 mousePos) override;
    void onFrame(glm::ivec2 mousePos, float deltaTime) override;
    void reset() override;

private:
    void clearMeasurement();
    void updateMeasurement(glm::ivec2 mousePos);
    void syncOverlayFromWorld();

    bool dragging_ = false;
    bool hasMeasurement_ = false;
    glm::vec3 startWorld_{};
    glm::vec3 endWorld_{};
};
