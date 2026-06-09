#include "LassoTool.h"

#include "App/interaction/picking/PickingSystem.h"
#include "Engine/Simulation.h"
#include "GUI/interface/UiState.h"
#include "GUI/io/keyboard/Keyboard.h"

LassoTool::LassoTool(ToolContext& context) noexcept : ITool(context) {}

void LassoTool::onLeftPressed(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    overlay.lassoVisible = true;
    overlay.lassoPoints.clear();
    overlay.lassoPoints.emplace_back(mousePos);
}

void LassoTool::onLeftReleased(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    // В GPU-режиме подтянуть свежие CPU-позиции из VRAM перед lasso-pick (Инкремент B
    // убрал безусловный per-frame download). processLasso читает AtomStorage::pos.
    // guard ctx.simulation как в FrameTool (по контракту всегда есть). В CPU-режиме no-op.
    if (ctx.simulation != nullptr) {
        ctx.simulation->syncFromGpuIfNeeded();
    }

    const bool cumulative = Keyboard::isPressed(GLFW_KEY_LEFT_CONTROL) || Keyboard::isPressed(GLFW_KEY_RIGHT_CONTROL);

    auto& overlay = ctx.pickingSystem->getOverlay();
    if (overlay.lassoVisible) {
        if (overlay.lassoPoints.empty() || overlay.lassoPoints.back() != mousePos) {
            overlay.lassoPoints.emplace_back(mousePos);
        }
        ctx.pickingSystem->processLasso(overlay.lassoPoints, cumulative);
        if (ctx.uiState != nullptr) {
            ctx.uiState->selectedAtomCount = static_cast<int>(ctx.pickingSystem->getSelectedAtomIds().size());
        }
    }
    overlay.reset();
}

void LassoTool::onFrame(glm::ivec2 mousePos, float deltaTime) {
    (void)deltaTime;

    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    if (!overlay.lassoVisible) {
        return;
    }

    constexpr float kMinStepSqr = 25.0f;
    if (overlay.lassoPoints.empty()) {
        overlay.lassoPoints.emplace_back(mousePos);
        return;
    }

    const glm::vec2 currentPos(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
    const auto& last = overlay.lassoPoints.back();
    const glm::vec2 lastPos(static_cast<float>(last.x), static_cast<float>(last.y));
    const glm::vec2 diff = currentPos - lastPos;
    if ((diff.x * diff.x + diff.y * diff.y) >= kMinStepSqr) {
        overlay.lassoPoints.emplace_back(mousePos);
    }
}

void LassoTool::reset() {
    ToolContext& ctx = context();
    if (ctx.pickingSystem != nullptr) {
        auto& overlay = ctx.pickingSystem->getOverlay();
        overlay.lassoVisible = false;
        overlay.lassoPoints.clear();
    }
}
