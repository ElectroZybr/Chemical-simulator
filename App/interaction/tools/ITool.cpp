#include "ITool.h"

#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/World.h"
#include "Rendering/BaseRenderer.h"

ITool::ITool(ToolContext& context) noexcept : context_(context) {}

ITool::~ITool() = default;

void ITool::onLeftPressed(Vec2i mousePos) { (void)mousePos; }

void ITool::onLeftReleased(Vec2i mousePos) { (void)mousePos; }

bool ITool::onRightPressed(Vec2i mousePos) {
    (void)mousePos;
    return false;
}

void ITool::onFrame(Vec2i mousePos, float deltaTime) {
    (void)mousePos;
    (void)deltaTime;
}

void ITool::reset() {}

Vec3f ITool::screenToWorld(Vec2i mousePos) const {
    if (BaseRenderer* renderer = context_.activeRenderer(); renderer != nullptr) {
        return renderer->camera.screenToWorld(mousePos);
    }
    return {};
}

Vec3f ITool::screenToLocalWorld(Vec2i mousePos) const {
    Vec3f worldPos = screenToWorld(mousePos);
    if (context_.simulation != nullptr) {
        worldPos -= context_.simulation->world().getRenderOffset();
    }
    return worldPos;
}

Vec2i ITool::worldToScreen(Vec3f worldPos) const {
    if (BaseRenderer* renderer = context_.activeRenderer(); renderer != nullptr) {
        return renderer->camera.worldToScreen(worldPos);
    }
    return {};
}
