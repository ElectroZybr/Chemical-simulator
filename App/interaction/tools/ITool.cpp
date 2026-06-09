#include "ITool.h"

#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/World.h"
#include "Rendering/BaseRenderer.h"

ITool::ITool(ToolContext& context) noexcept : context_(context) {}

ITool::~ITool() = default;

void ITool::onLeftPressed(glm::ivec2 mousePos) { (void)mousePos; }

void ITool::onLeftReleased(glm::ivec2 mousePos) { (void)mousePos; }

bool ITool::onRightPressed(glm::ivec2 mousePos) {
    (void)mousePos;
    return false;
}

void ITool::onFrame(glm::ivec2 mousePos, float deltaTime) {
    (void)mousePos;
    (void)deltaTime;
}

void ITool::reset() {}

glm::vec3 ITool::screenToWorld(glm::ivec2 mousePos) const {
    if (BaseRenderer* renderer = context_.activeRenderer(); renderer != nullptr) {
        return renderer->camera.screenToWorld(mousePos);
    }
    return {};
}

glm::vec3 ITool::screenToLocalWorld(glm::ivec2 mousePos) const {
    glm::vec3 worldPos = screenToWorld(mousePos);
    if (context_.simulation != nullptr) {
        worldPos -= context_.simulation->world().getRenderOffset();
    }
    return worldPos;
}

glm::ivec2 ITool::worldToScreen(glm::vec3 worldPos) const {
    if (BaseRenderer* renderer = context_.activeRenderer(); renderer != nullptr) {
        return renderer->camera.worldToScreen(worldPos);
    }
    return {};
}
