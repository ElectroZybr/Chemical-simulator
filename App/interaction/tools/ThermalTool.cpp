#include "ThermalTool.h"

#include <algorithm>
#include <cmath>

#include "Engine/Simulation.h"
#include "Rendering/BaseRenderer.h"
#include "Rendering/camera/Camera.h"

namespace {
    constexpr float kMinRadius = 0.25f;
    constexpr float kMaxRadius = 100.0f;
    constexpr float kMinStrength = 0.0f;
    constexpr float kMaxStrength = 10.0f;
}

ThermalTool::ThermalTool(ToolContext& context) noexcept : ITool(context) {}

void ThermalTool::onLeftPressed(Vec2i mousePos) {
    active_ = true;
    applyAt(mousePos, 1.0f / 60.0f);
}

void ThermalTool::onLeftReleased(Vec2i mousePos) {
    (void)mousePos;
    active_ = false;
}

void ThermalTool::onFrame(Vec2i mousePos, float deltaTime) {
    if (!active_) {
        return;
    }
    applyAt(mousePos, deltaTime);
}

void ThermalTool::reset() { active_ = false; }

void ThermalTool::setRadius(float radius) noexcept { radius_ = std::clamp(radius, kMinRadius, kMaxRadius); }

void ThermalTool::setStrength(float strength) noexcept { strength_ = std::clamp(strength, kMinStrength, kMaxStrength); }

void ThermalTool::applyAt(Vec2i mousePos, float deltaTime) {
    ToolContext& ctx = context();
    if (!ctx.isValid() || strength_ <= 0.0f) {
        return;
    }

    IRenderer* renderer = ctx.activeRenderer();
    if (renderer == nullptr) {
        return;
    }

    AtomStorage& atoms = ctx.simulation->atoms();
    if (atoms.empty()) {
        return;
    }

    const Vec3f center = screenToLocalWorld(mousePos);
    const float radiusSqr = radius_ * radius_;
    const bool is2D = renderer->camera.getMode() == Camera::Mode::Mode2D;

    float* const vx = atoms.vxData();
    float* const vy = atoms.vyData();
    float* const vz = atoms.vzData();

    const float sign = (mode_ == Mode::Heat) ? 1.0f : -1.0f;
    const float amount = strength_ * deltaTime;

    for (size_t i = 0; i < atoms.mobileCount(); ++i) {
        const Vec3f pos = atoms.pos(i);
        const float dx = static_cast<float>(pos.x - center.x);
        const float dy = static_cast<float>(pos.y - center.y);
        const float dz = is2D ? 0.0f : static_cast<float>(pos.z - center.z);
        const float distSqr = dx * dx + dy * dy + dz * dz;
        if (distSqr > radiusSqr) {
            continue;
        }

        const float dist = std::sqrt(std::max(distSqr, 0.0f));
        const float falloff = 1.0f - dist / radius_;
        const float factor = 1.0f + sign * amount * falloff;
        const float clampedFactor = std::max(0.0f, factor);

        vx[i] *= clampedFactor;
        vy[i] *= clampedFactor;
        vz[i] *= clampedFactor;
    }
}
