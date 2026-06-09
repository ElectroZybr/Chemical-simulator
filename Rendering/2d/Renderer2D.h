#pragma once

#include "Rendering/Render.h"

class Renderer2D : public RendererWGPU {
public:
    Renderer2D();
    ~Renderer2D() override = default;

protected:
    bool useLighting() override { return false; }
    void updateMatrices() override;
    glm::vec3 getLightDir() override { return glm::vec3(0.f); }
};
