#pragma once

#include <memory>

#include <GLFW/glfw3.h>

#include "Engine/math/Vec3.h"

class AtomStorage;
class IRenderer;
class PickingSystem;
class Simulation;
struct UiState;

struct ToolContext {
    GLFWwindow* window = nullptr;
    Simulation* simulation = nullptr;
    std::unique_ptr<IRenderer>* renderer = nullptr;
    PickingSystem* pickingSystem = nullptr;
    UiState* uiState = nullptr;

    [[nodiscard]] bool isValid() const noexcept { return window != nullptr && simulation != nullptr && renderer != nullptr; }

    [[nodiscard]] IRenderer* activeRenderer() const noexcept { return (renderer != nullptr) ? renderer->get() : nullptr; }
};

class ITool {
public:
    explicit ITool(ToolContext& context) noexcept;
    virtual ~ITool();

    ITool(const ITool&) = delete;
    ITool& operator=(const ITool&) = delete;

    virtual void onLeftPressed(Vec2i mousePos);
    virtual void onLeftReleased(Vec2i mousePos);
    virtual bool onRightPressed(Vec2i mousePos);
    virtual void onFrame(Vec2i mousePos, float deltaTime);
    virtual void reset();

protected:
    [[nodiscard]] ToolContext& context() noexcept { return context_; }
    [[nodiscard]] const ToolContext& context() const noexcept { return context_; }

    [[nodiscard]] Vec3f screenToWorld(Vec2i mousePos) const;
    [[nodiscard]] Vec3f screenToLocalWorld(Vec2i mousePos) const;
    [[nodiscard]] Vec2i worldToScreen(Vec3f worldPos) const;

private:
    ToolContext& context_;
};
