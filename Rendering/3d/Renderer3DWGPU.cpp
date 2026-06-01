#include "Renderer3DWGPU.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "generated/shaders/atom3d.wgsl.h"
#include "generated/shaders/grid.wgsl.h"
#include "generated/shaders/line.wgsl.h"

Renderer3DWGPU::Renderer3DWGPU() {
    initAtomPipeline(atom3dWGSL);
    initBoxPipeline(lineWGSL);
    initBondPipeline(lineWGSL);
    initGridPipeline(gridWGSL);

    camera.setMode(Camera::Mode::Orbit);
    camera.resetView();
}

void Renderer3DWGPU::updateMatrices() {
    const float aspect = static_cast<float>(camera.screenSize.x) / static_cast<float>(camera.screenSize.y);
    projection = glm::perspective(glm::radians(45.f), aspect, 0.1f, 1000.f);
    view = camera.getViewMatrix();
}

glm::vec3 Renderer3DWGPU::getLightDir() {
    const glm::vec3 eye = camera.getEyePosition();
    return glm::normalize(glm::vec3(view * glm::vec4(eye, 0.f)) + glm::vec3(25.f, 25.f, 0.f));
}
