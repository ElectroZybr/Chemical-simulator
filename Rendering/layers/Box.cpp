#include "Rendering/Renderer.h"

#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initBoxBuffer() {
    lineLayer_.boxVb = WGPUContext::instance().createBuffer(24 * 3 * sizeof(float), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Box_Geometry");
}

void RendererWGPU::drawBoxImpl(const glm::vec3& worldSize) {
    if (worldSize != lineLayer_.cachedBoxSize) {
        lineLayer_.cachedBoxSize = worldSize;
        const float x1 = lineLayer_.cachedBoxSize.x;
        const float y1 = lineLayer_.cachedBoxSize.y;
        const float z1 = lineLayer_.cachedBoxSize.z;
        lineLayer_.boxVertices = {
            0, y1, 0, x1, y1, 0, x1, y1, 0, x1, y1, z1, x1, y1, z1, 0,  y1, z1, 0, y1, z1, 0, y1, 0,
            0, 0,  0, x1, 0,  0, x1, 0,  0, x1, 0,  z1, x1, 0,  z1, 0,  0,  z1, 0, 0,  z1, 0, 0,  0,
            0, 0,  0, 0,  y1, 0, x1, 0,  0, x1, y1, 0,  x1, 0,  z1, x1, y1, z1, 0, 0,  z1, 0, y1, z1,
        };
        WGPUContext::instance().queue()->writeBuffer(*lineLayer_.boxVb, 0, lineLayer_.boxVertices.data(), sizeof(lineLayer_.boxVertices));
    }

    currentPass->setPipeline(*pipelines_.box);
    currentPass->setBindGroup(0, *lineLayer_.bindGroups[lineUniformSlotIndex_ - 1], 0, nullptr);
    currentPass->setVertexBuffer(0, *lineLayer_.boxVb, 0, sizeof(lineLayer_.boxVertices));
    currentPass->draw(24, 1, 0, 0);
}

void RendererWGPU::setLineColor(const glm::vec4& color) {
    if (lineUniformSlotIndex_ >= kLineUniformSlotCount) {
        lineUniformSlotIndex_ = kLineUniformSlotCount - 1;
    }

    SceneUniforms lineUniforms = currentSceneUniforms_;
    lineUniforms.lineColor = color;
    WGPUContext::instance().queue()->writeBuffer(*lineLayer_.uniformBuffers[lineUniformSlotIndex_], 0, &lineUniforms, sizeof(lineUniforms));
    ++lineUniformSlotIndex_;
}
