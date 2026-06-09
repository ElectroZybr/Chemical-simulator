#include "RemoveAtomTool.h"

#include <algorithm>
#include <vector>

#include "App/interaction/picking/PickingSystem.h"
#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "GUI/interface/UiState.h"

RemoveAtomTool::RemoveAtomTool(ToolContext& context) noexcept : ITool(context) {}

void RemoveAtomTool::onLeftPressed(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (ctx.simulation == nullptr || ctx.simulation->atoms().empty() || ctx.pickingSystem == nullptr) {
        return;
    }

    // В GPU-режиме подтянуть свежие CPU-позиции из VRAM ПЕРЕД pickAtom (Инкремент B
    // убрал безусловный per-frame download). removeAtom ниже синкает сам через
    // syncGpuBeforeEdit, но pickAtom читает позиции РАНЬШЕ — без этого синка он
    // попал бы в атом по устаревшим координатам. В CPU-режиме no-op.
    ctx.simulation->syncFromGpuIfNeeded();

    AtomHit hit;
    if (!ctx.pickingSystem->pickAtom(mousePos, 10.0f, hit)) {
        return;
    }

    const AtomStorage::AtomId targetId = hit.id;
    const auto& selected = ctx.pickingSystem->getSelectedAtomIds();
    const bool removeSelection = selected.contains(targetId);

    if (removeSelection) {
        std::vector<size_t> toRemove;
        toRemove.reserve(selected.size());
        for (const AtomStorage::AtomId atomId : selected) {
            const size_t index = ctx.simulation->atoms().indexOf(atomId);
            if (index < ctx.simulation->atoms().size()) {
                toRemove.push_back(index);
            }
        }
        std::sort(toRemove.begin(), toRemove.end(), std::greater<>());

        for (size_t index : toRemove) {
            ctx.simulation->removeAtom(index);
        }
    }
    else {
        const size_t targetIndex = ctx.simulation->atoms().indexOf(targetId);
        if (targetIndex < ctx.simulation->atoms().size()) {
            ctx.simulation->removeAtom(targetIndex);
        }
    }

    if (ctx.uiState != nullptr) {
        ctx.uiState->selectedAtomCount = static_cast<int>(ctx.pickingSystem->getSelectedAtomIds().size());
    }
}
