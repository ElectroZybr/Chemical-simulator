#include "Render.h"

#include <algorithm>
#include <vector>

#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::drawBoxImpl(const glm::vec3& worldSize) {
    if (worldSize != cachedBoxSize_) {
        cachedBoxSize_ = worldSize;
        const float x1 = cachedBoxSize_.x;
        const float y1 = cachedBoxSize_.y;
        const float z1 = cachedBoxSize_.z;
        boxVertices_ = {
            0, y1, 0, x1, y1, 0, x1, y1, 0, x1, y1, z1, x1, y1, z1, 0,  y1, z1, 0, y1, z1, 0, y1, 0,
            0, 0,  0, x1, 0,  0, x1, 0,  0, x1, 0,  z1, x1, 0,  z1, 0,  0,  z1, 0, 0,  z1, 0, 0,  0,
            0, 0,  0, 0,  y1, 0, x1, 0,  0, x1, y1, 0,  x1, 0,  z1, x1, y1, z1, 0, 0,  z1, 0, y1, z1,
        };
        WGPUContext::instance().queue()->writeBuffer(*boxVb, 0, boxVertices_.data(), sizeof(boxVertices_));
    }

    currentPass->setPipeline(*boxPipeline);
    currentPass->setBindGroup(0, *lineBindGroups[lineUniformSlotIndex_ - 1], 0, nullptr);
    currentPass->setVertexBuffer(0, *boxVb, 0, sizeof(boxVertices_));
    currentPass->draw(24, 1, 0, 0);
}

void RendererWGPU::setLineColor(const glm::vec4& color) {
    if (lineUniformSlotIndex_ >= kLineUniformSlotCount) {
        lineUniformSlotIndex_ = kLineUniformSlotCount - 1;
    }

    SceneUniforms lineUniforms = currentSceneUniforms_;
    lineUniforms.lineColor = color;
    WGPUContext::instance().queue()->writeBuffer(*lineUniformBuffers[lineUniformSlotIndex_], 0, &lineUniforms, sizeof(lineUniforms));
    ++lineUniformSlotIndex_;
}

void RendererWGPU::drawBondsImpl(const RenderAtomsView& atoms, const RenderBondsView& bonds) {
    if (bonds.empty()) {
        return;
    }

    struct BondVertexBuildContext {
        const RenderAtomsView* atoms = nullptr;
        std::vector<glm::vec3>* verts = nullptr;

        static void append(size_t aIndex, size_t bIndex, void* userData) {
            auto& ctx = *static_cast<BondVertexBuildContext*>(userData);
            const RenderAtomsView& atoms = *ctx.atoms;
            if (aIndex >= atoms.count || bIndex >= atoms.count || !atoms.hasPositions()) {
                return;
            }

            ctx.verts->emplace_back(atoms.x[aIndex], atoms.y[aIndex], atoms.z[aIndex]);
            ctx.verts->emplace_back(atoms.x[bIndex], atoms.y[bIndex], atoms.z[bIndex]);
        }
    };

    std::vector<glm::vec3> verts;
    verts.reserve(bonds.count * 2);
    BondVertexBuildContext buildContext{.atoms = &atoms, .verts = &verts};
    bonds.forEach(BondVertexBuildContext::append, &buildContext);
    if (verts.empty()) {
        return;
    }

    const uint64_t bytes = verts.size() * sizeof(glm::vec3);
    if (bytes > bondVbCapacity_) {
        bondVb = WGPUContext::instance().createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Bond_Geometry");
        bondVbCapacity_ = bytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*bondVb, 0, verts.data(), bytes);

    currentPass->setPipeline(*bondPipeline);
    currentPass->setBindGroup(0, *lineBindGroups[lineUniformSlotIndex_ - 1], 0, nullptr);
    currentPass->setVertexBuffer(0, *bondVb, 0, bytes);
    currentPass->draw(verts.size(), 1, 0, 0);
}

void RendererWGPU::drawGridImpl(const RenderGridView& grid) {
    struct GridBuildContext {
        RendererWGPU* renderer = nullptr;
        float maxCount = 1.0f;

        static void append(const RenderGridCell& cell, void* userData) {
            auto& ctx = *static_cast<GridBuildContext*>(userData);
            ctx.renderer->gridData.push_back(GridInstance{
                .origin = glm::vec4(cell.origin, 0.0f),
                .cellSize = cell.cellSize,
                .atomCount = cell.atomCount,
            });
            ctx.maxCount = std::max(ctx.maxCount, cell.atomCount);
        }
    };

    gridData.clear();
    gridData.reserve(grid.count);
    GridBuildContext buildContext{.renderer = this};
    grid.forEach(GridBuildContext::append, &buildContext);
    if (gridData.empty()) {
        return;
    }

    float mc = buildContext.maxCount;
    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, offsetof(SceneUniforms, maxCount), &mc, sizeof(float));

    const uint64_t instBytes = gridData.size() * sizeof(GridInstance);
    if (!gridInstVb || instBytes > gridInstVbCapacity_) {
        gridInstVb =
            WGPUContext::instance().createBuffer(instBytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Grid_Instances");
        gridInstVbCapacity_ = instBytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*gridInstVb, 0, gridData.data(), instBytes);

    currentPass->setPipeline(*gridPipeline);
    currentPass->setBindGroup(0, *gridBindGroup, 0, nullptr);
    currentPass->setVertexBuffer(0, *gridLineVb, 0, gridLineVb->getSize());
    currentPass->setVertexBuffer(1, *gridInstVb, 0, instBytes);
    currentPass->draw(24, gridData.size(), 0, 0);
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
    if (bytes > memoryOrderVbCapacity_) {
        memoryOrderVb =
            WGPUContext::instance().createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Memory_Order_Geometry");
        memoryOrderVbCapacity_ = bytes * 2;
    }

    WGPUContext::instance().queue()->writeBuffer(*memoryOrderVb, 0, verts.data(), bytes);

    currentPass->setPipeline(*memoryOrderPipeline);
    currentPass->setBindGroup(0, *lineBindGroups[lineUniformSlotIndex_ - 1], 0, nullptr);
    currentPass->setVertexBuffer(0, *memoryOrderVb, 0, bytes);
    currentPass->draw(verts.size(), 1, 0, 0);
}
