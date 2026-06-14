#pragma once

#include <glm/gtc/matrix_transform.hpp>

#include "Rendering/Renderer.h"
#include "generated/shaders/atom2d.wgsl.h"

class Renderer2D : public RendererWGPU {
public:
    Renderer2D() {
        initAtomSpherePipeline(atom2dWGSL);
        initSharedPipelines();

        camera.setMode(Camera::Mode::Mode2D);
        camera.resetView();
    }

    ~Renderer2D() override = default;

protected:
    bool useLighting() override { return false; }

    void updateMatrices() override {
        projection = camera.getProjectionMatrix();
        view = camera.getViewMatrix();
    }

    glm::vec3 getLightDir() override { return glm::vec3(0.f); }
};
