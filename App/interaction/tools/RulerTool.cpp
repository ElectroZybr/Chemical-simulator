#include "RulerTool.h"

#include <cstdio>
#include <string>

#include "App/interaction/picking/PickingSystem.h"
#include "Lattice/Engine/Consts.h"
#include "GUI/interface/UiState.h"
#include "Rendering/BaseRenderer.h"

namespace {
    glm::vec3 mapMouseToRulerWorld(const ToolContext& ctx, glm::ivec2 mousePos) {
        BaseRenderer* renderer = ctx.activeRenderer();
        if (renderer == nullptr) {
            return glm::vec3(0.0f);
        }

        return renderer->camera.screenToWorld(mousePos);
    }

    std::string makeRulerTooltip(const glm::vec3& start, const glm::vec3& end) {
        const float distanceAngstrom = glm::length(end - start);
        const float distanceNm = distanceAngstrom * static_cast<float>(Units::AngstromToNm);

        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Distance: %.2f A (%.2f nm)", distanceAngstrom, distanceNm);
        return buffer;
    }
}

RulerTool::RulerTool(ToolContext& context) noexcept : ITool(context) {}

void RulerTool::onLeftPressed(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    overlay.rulerVisible = true;
    startWorld_ = mapMouseToRulerWorld(ctx, mousePos);
    endWorld_ = startWorld_;
    hasMeasurement_ = true;
    dragging_ = true;
    syncOverlayFromWorld();

    if (ctx.uiState != nullptr) {
        ctx.uiState->drawToolTrip = false;
        ctx.uiState->toolTooltipText.clear();
    }
    overlay.rulerLabel = makeRulerTooltip(startWorld_, endWorld_);
}

void RulerTool::onLeftReleased(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    endWorld_ = mapMouseToRulerWorld(ctx, mousePos);
    dragging_ = false;
    syncOverlayFromWorld();
    if (ctx.uiState != nullptr) {
        ctx.uiState->drawToolTrip = false;
        ctx.uiState->toolTooltipText.clear();
    }
}

bool RulerTool::onRightPressed(glm::ivec2 mousePos) {
    (void)mousePos;
    return false;
}

void RulerTool::onFrame(glm::ivec2 mousePos, float deltaTime) {
    (void)deltaTime;
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    if (!overlay.rulerVisible || !hasMeasurement_) {
        return;
    }

    if (dragging_) {
        updateMeasurement(mousePos);
    }
    else {
        syncOverlayFromWorld();
    }
}

void RulerTool::reset() {
    dragging_ = false;
    hasMeasurement_ = false;
    ToolContext& ctx = context();
    if (ctx.uiState != nullptr) {
        ctx.uiState->drawToolTrip = false;
        ctx.uiState->toolTooltipText.clear();
    }
}

void RulerTool::clearMeasurement() {
    ToolContext& ctx = context();
    if (ctx.pickingSystem != nullptr) {
        auto& overlay = ctx.pickingSystem->getOverlay();
        overlay.rulerVisible = false;
        overlay.rulerLabel.clear();
    }
    hasMeasurement_ = false;
    reset();
}

void RulerTool::updateMeasurement(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    if (!overlay.rulerVisible) {
        return;
    }

    endWorld_ = mapMouseToRulerWorld(ctx, mousePos);
    syncOverlayFromWorld();
    if (ctx.uiState != nullptr) {
        ctx.uiState->drawToolTrip = false;
        ctx.uiState->toolTooltipText.clear();
    }
    overlay.rulerLabel = makeRulerTooltip(startWorld_, endWorld_);
}

void RulerTool::syncOverlayFromWorld() {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr || !hasMeasurement_) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    BaseRenderer* renderer = ctx.activeRenderer();
    if (renderer == nullptr) {
        return;
    }

    overlay.rulerStart = renderer->camera.worldToScreen(startWorld_);
    overlay.rulerEnd = renderer->camera.worldToScreen(endWorld_);
}
