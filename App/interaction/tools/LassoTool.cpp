#include "LassoTool.h"

#include "App/interaction/picking/PickingSystem.h"
#include "Engine/Simulation.h"
#include "GUI/interface/UiState.h"
#include "GUI/io/keyboard/Keyboard.h"

LassoTool::LassoTool(ToolContext& context) noexcept : ITool(context) {}

void LassoTool::onLeftPressed(Vec2i mousePos) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    overlay.lassoVisible = true;
    overlay.lassoPoints.clear();
    overlay.lassoPoints.emplace_back(mousePos);
}

void LassoTool::onLeftReleased(Vec2i mousePos) {
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
        if (overlay.lassoPoints.empty() || overlay.lassoPoints.back() != Vec2i(mousePos)) {
            overlay.lassoPoints.emplace_back(mousePos);
        }
        ctx.pickingSystem->processLasso(overlay.lassoPoints, cumulative);
        if (ctx.uiState != nullptr) {
            ctx.uiState->selectedAtomCount = static_cast<int>(ctx.pickingSystem->getSelectedIndices().size());
        }
    }
    overlay.reset();
}

void LassoTool::onFrame(Vec2i mousePos, float deltaTime) {
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

    const Vec2f currentPos(mousePos.x, mousePos.y);
    const Vec2f lastPos(overlay.lassoPoints.back().x, overlay.lassoPoints.back().y);
    if ((currentPos - lastPos).sqrAbs() >= kMinStepSqr) {
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
