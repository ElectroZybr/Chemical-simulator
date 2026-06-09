#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "Renderer2D.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Rendering/Render.h"
#include "generated/shaders/atom2d.wgsl.h"
#include "generated/shaders/grid.wgsl.h"
#include "generated/shaders/line.wgsl.h"
#include "generated/shaders/memory_order.wgsl.h"

Renderer2D::Renderer2D() {
    initAtomPipeline(atom2dWGSL);
    initBoxPipeline(lineWGSL);
    initBondPipeline(lineWGSL);
    initMemoryOrderPipeline(memory_orderWGSL);
    initGridPipeline(gridWGSL);

    camera.setMode(Camera::Mode::Mode2D);
    camera.resetView();
}

void Renderer2D::updateMatrices() {
    const float aspect = static_cast<float>(camera.screenSize.x) / static_cast<float>(camera.screenSize.y);
    const float viewWidth = static_cast<float>(camera.screenSize.x) / camera.getZoom();
    const float viewHeight = viewWidth / aspect;

    projection = glm::ortho(-viewWidth / 2.f, viewWidth / 2.f, -viewHeight / 2.f, viewHeight / 2.f, -10000.f, 10000.f);
    view = glm::translate(glm::mat4(1.f), glm::vec3(-camera.getPosition().x, -camera.getPosition().y, 0.f));
}
