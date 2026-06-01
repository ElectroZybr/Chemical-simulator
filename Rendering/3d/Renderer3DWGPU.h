#pragma once

#include "Rendering/RendererWGPU.h"

class Renderer3DWGPU : public RendererWGPU {
public:
    Renderer3DWGPU();
    ~Renderer3DWGPU() override = default;

protected:
    void updateMatrices() override;
    glm::vec3 getLightDir() override;
    bool useLighting() override { return true; }
};
