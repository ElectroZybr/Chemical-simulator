#include "FrameTool.h"

#include "App/interaction/picking/PickingSystem.h"
#include "Engine/Simulation.h"
#include "GUI/interface/UiState.h"
#include "GUI/io/keyboard/Keyboard.h"

FrameTool::FrameTool(ToolContext& context) noexcept : ITool(context) {}

void FrameTool::onLeftPressed(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    overlay.boxVisible = true;
    overlay.boxStart = mousePos;
    overlay.boxEnd = mousePos;
}

void FrameTool::onLeftReleased(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    // В GPU-режиме подтянуть свежие CPU-позиции из VRAM перед rect-pick (Инкремент B
    // убрал безусловный per-frame download). processRect читает AtomStorage::pos.
    // guard ctx.simulation: по контракту он всегда есть (ToolContext::isValid), но
    // onLeftReleased проверяет только pickingSystem — guard дёшев. В CPU-режиме no-op.
    if (ctx.simulation != nullptr) {
        ctx.simulation->syncFromGpuIfNeeded();
    }

    const bool cumulative = Keyboard::isPressed(GLFW_KEY_LEFT_CONTROL) || Keyboard::isPressed(GLFW_KEY_RIGHT_CONTROL);

    auto& overlay = ctx.pickingSystem->getOverlay();
    if (overlay.boxVisible) {
        ctx.pickingSystem->processRect(overlay.boxStart, mousePos, cumulative);
        if (ctx.uiState != nullptr) {
            ctx.uiState->selectedAtomCount = static_cast<int>(ctx.pickingSystem->getSelectedAtomIds().size());
        }
    }
    overlay.reset();
}

void FrameTool::onFrame(glm::ivec2 mousePos, float) {
    ToolContext& ctx = context();
    if (ctx.pickingSystem == nullptr) {
        return;
    }

    auto& overlay = ctx.pickingSystem->getOverlay();
    if (overlay.boxVisible) {
        overlay.boxEnd = mousePos;
    }
}

void FrameTool::reset() {
    ToolContext& ctx = context();
    if (ctx.pickingSystem != nullptr) {
        ctx.pickingSystem->getOverlay().boxVisible = false;
    }
}
