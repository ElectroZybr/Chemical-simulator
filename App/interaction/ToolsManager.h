#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "App/interaction/picking/PickingSystem.h"
#include "App/interaction/selection/NeighborListOverlay.h"
#include "App/interaction/tools/ITool.h"
#include "Lattice/Engine/Simulation.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "Rendering/BaseRenderer.h"

class World;
class SideToolsPanel;
class Interface;
struct UiState;

class ToolsManager {
public:
    class Overlay {
    public:
        void draw();

    private:
        NeighborListOverlay neighborListOverlay_;
    };

    enum class Mode : uint8_t {
        Cursor,
        Frame,
        Lasso,
        Ruler,
        AddAtom,
        RemoveAtom,
    };

    static void init(GLFWwindow* window, Lattice::Simulation& simulation, std::unique_ptr<BaseRenderer>& renderer, Interface& appInterface);

    static glm::vec3 screenToWorld(glm::ivec2 mousePos);
    static glm::ivec2 worldToScreen(glm::vec3 pos);

    static void onLeftPressed(glm::ivec2 mousePos);
    static void onLeftReleased(glm::ivec2 mousePos);
    static bool onRightPressed(glm::ivec2 mousePos);
    static void onFrame(glm::ivec2 mousePos, float deltaTime);
    static void resetInteractionState();
    static bool isInteractingNow() noexcept;
    static bool blocksCameraControls() noexcept;

    static Mode currentMode();
    static bool isSelectionMode(Mode mode);

    static Overlay overlay;
    static PickingSystem* pickingSystem;

private:
    static constexpr size_t kModeCount = 6;

    static ITool* activeTool() noexcept;
    static void syncToolMode() noexcept;
    static void syncPickingWorldToActive(bool clearSelection);
    static void selectWorldAt(glm::ivec2 mousePos);
    static size_t toIndex(Mode mode) noexcept;

    static GLFWwindow* window;
    static std::unique_ptr<BaseRenderer>* renderer;
    static Lattice::Simulation* simulation;
    static UiState* uiState;
    static SideToolsPanel* sideToolsPanel;
    static ToolContext toolContext;
    static std::array<std::unique_ptr<ITool>, kModeCount> toolInstances;
    static Mode syncedMode;
    static Lattice::Simulation::WorldId pickingWorldId;

    static glm::ivec2 startMousePos;
    static glm::ivec2 lastSceneMousePos;
    static bool isInteracting;
};
