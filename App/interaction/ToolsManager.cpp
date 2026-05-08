#include "ToolsManager.h"

#include "App/interaction/tools/AddAtomTool.h"
#include "App/interaction/tools/CursorTool.h"
#include "App/interaction/tools/FrameTool.h"
#include "App/interaction/tools/LassoTool.h"
#include "App/interaction/tools/RemoveAtomTool.h"
#include "App/interaction/tools/RulerTool.h"
#include "GUI/interface/interface.h"
#include "GUI/interface/panels/tools/SideToolsPanel.h"

namespace {
    ToolsManager::Mode mapPanelTool(SideToolsPanel::Tool tool) {
        switch (tool) {
        case SideToolsPanel::Tool::Cursor:
            return ToolsManager::Mode::Cursor;
        case SideToolsPanel::Tool::Frame:
            return ToolsManager::Mode::Frame;
        case SideToolsPanel::Tool::Lasso:
            return ToolsManager::Mode::Lasso;
        case SideToolsPanel::Tool::Ruler:
            return ToolsManager::Mode::Ruler;
        case SideToolsPanel::Tool::AddAtom:
            return ToolsManager::Mode::AddAtom;
        case SideToolsPanel::Tool::RemoveAtom:
            return ToolsManager::Mode::RemoveAtom;
        }
        return ToolsManager::Mode::Cursor;
    }
}

GLFWwindow* ToolsManager::window = nullptr;
std::unique_ptr<IRenderer>* ToolsManager::renderer = nullptr;
PickingSystem* ToolsManager::pickingSystem = nullptr;
Simulation* ToolsManager::simulation = nullptr;
UiState* ToolsManager::uiState = nullptr;
SideToolsPanel* ToolsManager::sideToolsPanel = nullptr;
ToolContext ToolsManager::toolContext = {};
std::array<std::unique_ptr<ITool>, ToolsManager::kModeCount> ToolsManager::toolInstances = {};
ToolsManager::Mode ToolsManager::syncedMode = ToolsManager::Mode::Cursor;
Vec2i ToolsManager::startMousePos = {};
Vec2i ToolsManager::lastSceneMousePos = {};
bool ToolsManager::isInteracting = false;

void ToolsManager::init(GLFWwindow* w, Simulation& sim, std::unique_ptr<IRenderer>& rend, Interface& appInterface) {
    window = w;
    simulation = &sim;
    renderer = &rend;
    uiState = &appInterface.state();
    sideToolsPanel = &appInterface.sideToolsPanel;

    delete pickingSystem;
    pickingSystem = new PickingSystem(simulation->atoms(), simulation->world(), *renderer);

    toolContext.window = w;
    toolContext.simulation = &sim;
    toolContext.renderer = &rend;
    toolContext.pickingSystem = pickingSystem;
    toolContext.uiState = &appInterface.state();

    for (auto& tool : toolInstances) {
        tool.reset();
    }
    toolInstances[toIndex(Mode::Cursor)] = std::make_unique<CursorTool>(toolContext);
    toolInstances[toIndex(Mode::Frame)] = std::make_unique<FrameTool>(toolContext);
    toolInstances[toIndex(Mode::Lasso)] = std::make_unique<LassoTool>(toolContext);
    toolInstances[toIndex(Mode::Ruler)] = std::make_unique<RulerTool>(toolContext);
    toolInstances[toIndex(Mode::AddAtom)] = std::make_unique<AddAtomTool>(toolContext);
    toolInstances[toIndex(Mode::RemoveAtom)] = std::make_unique<RemoveAtomTool>(toolContext);
    syncedMode = currentMode();
    isInteracting = false;
    lastSceneMousePos = {};
}

void ToolsManager::resetInteractionState() {
    for (auto& tool : toolInstances) {
        if (tool) {
            tool->reset();
        }
    }

    if (pickingSystem) {
        pickingSystem->clearSelection();
        pickingSystem->getOverlay().reset();
    }

    if (uiState != nullptr) {
        uiState->selectedAtomCount = 0;
        uiState->drawToolTrip = false;
        uiState->toolTooltipText.clear();
    }
    isInteracting = false;
}

bool ToolsManager::isInteractingNow() noexcept { return isInteracting; }

void ToolsManager::onLeftPressed(Vec2i mousePos) {
    if ((uiState != nullptr && uiState->cursorHovered) || !renderer || !renderer->get() || !pickingSystem) {
        return;
    }

    syncToolMode();
    startMousePos = mousePos;
    lastSceneMousePos = mousePos;
    isInteracting = true;

    if (ITool* tool = activeTool(); tool != nullptr) {
        tool->onLeftPressed(mousePos);
    }
}

void ToolsManager::onLeftReleased(Vec2i mousePos) {
    syncToolMode();
    if (!isInteracting) {
        return;
    }

    const bool cursorHovered = uiState != nullptr && uiState->cursorHovered;
    const Vec2i releasePos = cursorHovered ? lastSceneMousePos : mousePos;

    if (ITool* tool = activeTool(); tool != nullptr) {
        tool->onLeftReleased(releasePos);
    }
    isInteracting = false;
}

bool ToolsManager::onRightPressed(Vec2i mousePos) {
    if (uiState != nullptr && uiState->cursorHovered) {
        return false;
    }
    syncToolMode();

    if (ITool* tool = activeTool(); tool != nullptr) {
        return tool->onRightPressed(mousePos);
    }
    return false;
}

void ToolsManager::onFrame(Vec2i mousePos, float deltaTime) {
    if (!renderer || !renderer->get() || !simulation || !pickingSystem) {
        return;
    }

    syncToolMode();
    if (uiState != nullptr && uiState->cursorHovered) {
        return;
    }

    lastSceneMousePos = mousePos;

    if (ITool* tool = activeTool(); tool != nullptr) {
        tool->onFrame(mousePos, deltaTime);
    }

    if (currentMode() == Mode::Cursor) {
        if (uiState != nullptr) {
            uiState->drawToolTrip = false;
            uiState->toolTooltipText.clear();
        }
    }
}

Vec3f ToolsManager::screenToWorld(Vec2i mousePos) { return (*renderer)->camera.screenToWorld(mousePos); }

Vec2i ToolsManager::worldToScreen(Vec3f pos) { return (*renderer)->camera.worldToScreen(pos); }

ToolsManager::Mode ToolsManager::currentMode() {
    if (sideToolsPanel == nullptr) {
        return Mode::Cursor;
    }
    return mapPanelTool(sideToolsPanel->getSelectedTool());
}

bool ToolsManager::isSelectionMode(ToolsManager::Mode mode) { return mode == Mode::Frame || mode == Mode::Lasso; }

ITool* ToolsManager::activeTool() noexcept { return toolInstances[toIndex(currentMode())].get(); }

void ToolsManager::syncToolMode() noexcept {
    const Mode mode = currentMode();
    if (mode == syncedMode) {
        return;
    }

    if (toolInstances[toIndex(syncedMode)]) {
        toolInstances[toIndex(syncedMode)]->reset();
    }
    if (pickingSystem) {
        pickingSystem->getOverlay().reset();
    }
    if (uiState != nullptr) {
        uiState->drawToolTrip = false;
        uiState->toolTooltipText.clear();
    }
    isInteracting = false;
    syncedMode = mode;
}

size_t ToolsManager::toIndex(Mode mode) noexcept { return static_cast<size_t>(mode); }
