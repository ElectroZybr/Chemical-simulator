#include "Rendering/Renderer.h"

#include <vector>

#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initMemoryOrderBuffer() {
    lineLayer_.memoryOrderVb = WGPUContext::instance().createBuffer(128, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Memory_Order_Geometry");
    lineLayer_.memoryOrderVbCapacity = 128;
}

void RendererWGPU::drawMemoryOrderImpl(const RenderAtomsView& atoms) {
    if (atoms.count < 2 || !atoms.hasPositions()) {
        return;
    }

    std::vector<MemoryOrderVertex> verts;
    verts.reserve((atoms.count - 1) * 2);

    const float invLastIndex = atoms.count > 1 ? 1.0f / static_cast<float>(atoms.count - 1) : 0.0f;
    for (size_t i = 0; i + 1 < atoms.count; ++i) {
        verts.push_back(MemoryOrderVertex{
            .pos = glm::vec3(atoms.x[i], atoms.y[i], atoms.z[i]),
            .t = static_cast<float>(i) * invLastIndex,
        });
        verts.push_back(MemoryOrderVertex{
            .pos = glm::vec3(atoms.x[i + 1], atoms.y[i + 1], atoms.z[i + 1]),
            .t = static_cast<float>(i + 1) * invLastIndex,
        });
    }

    const uint64_t bytes = verts.size() * sizeof(MemoryOrderVertex);
    if (bytes > lineLayer_.memoryOrderVbCapacity) {
        lineLayer_.memoryOrderVb =
            WGPUContext::instance().createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Memory_Order_Geometry");
        lineLayer_.memoryOrderVbCapacity = bytes * 2;
    }

    WGPUContext::instance().queue()->writeBuffer(*lineLayer_.memoryOrderVb, 0, verts.data(), bytes);

    currentPass->setPipeline(*pipelines_.memoryOrder);
    currentPass->setBindGroup(0, *lineLayer_.bindGroups[lineUniformSlotIndex_ - 1], 0, nullptr);
    currentPass->setVertexBuffer(0, *lineLayer_.memoryOrderVb, 0, bytes);
    currentPass->draw(verts.size(), 1, 0, 0);
}
