#pragma once

#include <memory>

#include <GLFW/glfw3.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

class AtomStorage;
class BaseRenderer;
class PickingSystem;
namespace Lattice {
    class Simulation;
}
struct UiState;

struct ToolContext {
    GLFWwindow* window = nullptr;
    Lattice::Simulation* simulation = nullptr;
    std::unique_ptr<BaseRenderer>* renderer = nullptr;
    PickingSystem* pickingSystem = nullptr;
    UiState* uiState = nullptr;

    [[nodiscard]] bool isValid() const noexcept { return window != nullptr && simulation != nullptr && renderer != nullptr; }

    [[nodiscard]] BaseRenderer* activeRenderer() const noexcept { return (renderer != nullptr) ? renderer->get() : nullptr; }
};

class ITool {
public:
    explicit ITool(ToolContext& context) noexcept;
    virtual ~ITool();

    ITool(const ITool&) = delete;
    ITool& operator=(const ITool&) = delete;

    virtual void onLeftPressed(glm::ivec2 mousePos);
    virtual void onLeftReleased(glm::ivec2 mousePos);
    virtual bool onRightPressed(glm::ivec2 mousePos);
    virtual void onFrame(glm::ivec2 mousePos, float deltaTime);
    virtual void reset();

protected:
    [[nodiscard]] ToolContext& context() noexcept { return context_; }
    [[nodiscard]] const ToolContext& context() const noexcept { return context_; }

    [[nodiscard]] glm::vec3 screenToWorld(glm::ivec2 mousePos) const;
    [[nodiscard]] glm::vec3 screenToLocalWorld(glm::ivec2 mousePos) const;
    [[nodiscard]] glm::ivec2 worldToScreen(glm::vec3 worldPos) const;

private:
    ToolContext& context_;
};
