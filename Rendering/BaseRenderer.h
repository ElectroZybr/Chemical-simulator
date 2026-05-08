#pragma once

#include <cstdint>

#include <webgpu/webgpu-raii.hpp>

#include "Engine/World.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Rendering/camera/Camera.h"

class IRenderer {
public:
    enum class SpeedColorMode : uint8_t {
        AtomColor = 0,
        GradientClassic = 1,
        GradientTurbo = 2,
    };

    virtual ~IRenderer() = default;

    virtual void drawShot(wgpu::TextureView targetView, wgpu::TextureView depthView, const AtomStorage& atoms, const Bond::List& bonds,
                          const World& box) = 0;
    virtual void endFrame() = 0;

    bool drawGrid = false;
    bool drawBonds = false;
    SpeedColorMode speedColorMode = SpeedColorMode::AtomColor;
    float speedGradientMax = 5.0f;
    float alpha = 0.05f;

    Camera camera;

protected:
    IRenderer(World& box) : camera(box) {}
};
