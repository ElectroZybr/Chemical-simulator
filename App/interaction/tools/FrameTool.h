#pragma once

#include "App/interaction/tools/ITool.h"

class FrameTool final : public ITool {
public:
    explicit FrameTool(ToolContext& context) noexcept;

    void onLeftPressed(glm::ivec2 mousePos) override;
    void onLeftReleased(glm::ivec2 mousePos) override;
    void onFrame(glm::ivec2 mousePos, float deltaTime) override;
    void reset() override;
};
