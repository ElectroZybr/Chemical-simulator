#pragma once

#include <glm/gtc/matrix_transform.hpp>

#include "Rendering/Renderer.h"
#include "generated/shaders/atom3d.wgsl.h"

class Renderer3D : public RendererWGPU {
public:
    Renderer3D() {
        initAtomSpherePipeline(atom3dWGSL);
        initSharedPipelines();

        camera.setMode(Camera::Mode::Orbit);
        camera.resetView();
    }

    ~Renderer3D() override = default;

protected:
    void updateMatrices() override {
        const float aspect = static_cast<float>(camera.screenSize.x) / static_cast<float>(camera.screenSize.y);
        projection = glm::perspectiveRH_ZO(glm::radians(45.f), aspect, 0.1f, 1000.f);
        view = camera.getViewMatrix();
    }

    glm::vec3 getLightDir() override {
        const glm::vec3 eye = camera.getEyePosition();
        return glm::normalize(glm::vec3(view * glm::vec4(eye, 0.f)) + glm::vec3(25.f, 25.f, 0.f));
    }

    bool useLighting() override { return true; }
};
