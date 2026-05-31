#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "App/interaction/picking/PickingSystem.h"
#include "App/interaction/selection/NeighborListOverlay.h"
#include "App/interaction/tools/ITool.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
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

    static void init(GLFWwindow* window, Simulation& simulation, std::unique_ptr<IRenderer>& renderer, Interface& appInterface);

    static Vec3f screenToWorld(Vec2i mousePos);
    static Vec2i worldToScreen(Vec3f pos);

    static void onLeftPressed(Vec2i mousePos);
    static void onLeftReleased(Vec2i mousePos);
    static bool onRightPressed(Vec2i mousePos);
    static void onFrame(Vec2i mousePos, float deltaTime);
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
    static void selectWorldAt(Vec2i mousePos);
    static size_t toIndex(Mode mode) noexcept;

    static GLFWwindow* window;
    static std::unique_ptr<IRenderer>* renderer;
    static Simulation* simulation;
    static UiState* uiState;
    static SideToolsPanel* sideToolsPanel;
    static ToolContext toolContext;
    static std::array<std::unique_ptr<ITool>, kModeCount> toolInstances;
    static Mode syncedMode;
    static Simulation::WorldId pickingWorldId;

    static Vec2i startMousePos;
    static Vec2i lastSceneMousePos;
    static bool isInteracting;
};
