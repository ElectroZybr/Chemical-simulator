#pragma once

#include "Rendering/Render.h"

class Renderer3D : public RendererWGPU {
public:
    Renderer3D();
    ~Renderer3D() override = default;

protected:
    void updateMatrices() override;
    glm::vec3 getLightDir() override;
    bool useLighting() override { return true; }
};
