#pragma once

#include "Rendering/RendererWGPU.h"

class Renderer2DWGPU : public RendererWGPU {
public:
    Renderer2DWGPU(World& simbox, wgpu::TextureFormat surfaceFormat);
    ~Renderer2DWGPU() override = default;

protected:
    bool useLighting() override { return false; }
    void updateMatrices() override;
    glm::vec3 getLightDir() override { return glm::vec3(0.f); }
};
