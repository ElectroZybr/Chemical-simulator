#include "Rendering/Renderer.h"

#include <algorithm>

#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initGridLineBuffer() {
    const float lines[] = {
        0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
        1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1,
    };
    WGPUContext& ctx = WGPUContext::instance();
    gridLayer_.gridLineVb = ctx.createBuffer(sizeof(lines), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Grid_Cell_Unit_Lines");
    ctx.queue()->writeBuffer(*gridLayer_.gridLineVb, 0, lines, sizeof(lines));
}

void RendererWGPU::drawGridImpl(const RenderGridView& grid) {
    struct GridBuildContext {
        RendererWGPU* renderer = nullptr;
        float maxCount = 1.0f;

        static void append(const RenderGridCell& cell, void* userData) {
            auto& ctx = *static_cast<GridBuildContext*>(userData);
            ctx.renderer->gridLayer_.gridData.push_back(GridInstance{
                .origin = glm::vec4(cell.origin, 0.0f),
                .cellSize = cell.cellSize,
                .atomCount = cell.atomCount,
            });
            ctx.maxCount = std::max(ctx.maxCount, cell.atomCount);
        }
    };

    gridLayer_.gridData.clear();
    gridLayer_.gridData.reserve(grid.count);
    GridBuildContext buildContext{.renderer = this};
    grid.forEach(GridBuildContext::append, &buildContext);
    if (gridLayer_.gridData.empty()) {
        return;
    }

    if (gridUniformSlotIndex_ >= kGridUniformSlotCount) {
        gridUniformSlotIndex_ = kGridUniformSlotCount - 1;
    }
    const size_t uniformSlot = gridUniformSlotIndex_++;
    SceneUniforms gridUniforms = currentSceneUniforms_;
    gridUniforms.maxCount.x = buildContext.maxCount;
    WGPUContext::instance().queue()->writeBuffer(*gridLayer_.uniformBuffers[uniformSlot], 0, &gridUniforms, sizeof(gridUniforms));

    const uint64_t instBytes = gridLayer_.gridData.size() * sizeof(GridInstance);
    if (!gridLayer_.gridInstVb || instBytes > gridLayer_.gridInstVbCapacity) {
        gridLayer_.gridInstVb =
            WGPUContext::instance().createBuffer(instBytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Grid_Instances");
        gridLayer_.gridInstVbCapacity = instBytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*gridLayer_.gridInstVb, 0, gridLayer_.gridData.data(), instBytes);

    currentPass->setPipeline(*pipelines_.grid);
    currentPass->setBindGroup(0, *gridLayer_.bindGroups[uniformSlot], 0, nullptr);
    currentPass->setVertexBuffer(0, *gridLayer_.gridLineVb, 0, gridLayer_.gridLineVb->getSize());
    currentPass->setVertexBuffer(1, *gridLayer_.gridInstVb, 0, instBytes);
    currentPass->draw(24, gridLayer_.gridData.size(), 0, 0);
}
