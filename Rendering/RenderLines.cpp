#include "Render.h"

#include <algorithm>
#include <cmath>
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

    if (gridUniformSlotIndex_ >= kGridUniformSlotCount) {
        gridUniformSlotIndex_ = kGridUniformSlotCount - 1;
    }
    const size_t uniformSlot = gridUniformSlotIndex_++;
    SceneUniforms gridUniforms = currentSceneUniforms_;
    gridUniforms.maxCount.x = buildContext.maxCount;
    WGPUContext::instance().queue()->writeBuffer(*gridUniformBuffers[uniformSlot], 0, &gridUniforms, sizeof(gridUniforms));

    const uint64_t instBytes = gridData.size() * sizeof(GridInstance);
    if (!gridInstVb || instBytes > gridInstVbCapacity_) {
        gridInstVb =
            WGPUContext::instance().createBuffer(instBytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Grid_Instances");
        gridInstVbCapacity_ = instBytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*gridInstVb, 0, gridData.data(), instBytes);

    currentPass->setPipeline(*gridPipeline);
    currentPass->setBindGroup(0, *gridBindGroups[uniformSlot], 0, nullptr);
    currentPass->setVertexBuffer(0, *gridLineVb, 0, gridLineVb->getSize());
    currentPass->setVertexBuffer(1, *gridInstVb, 0, instBytes);
    currentPass->draw(24, gridData.size(), 0, 0);
}

void RendererWGPU::drawVectorFieldImpl(const RenderData& renderData) {
    const RenderVectorFieldView& field = renderData.vectorField;
    fieldData.clear();
    fieldData.reserve(field.cellCount());
    float maxValue = 1.0f;
    for (int y = 0; y + 1 < field.gridSize.y; ++y) {
        for (int x = 0; x + 1 < field.gridSize.x; ++x) {
            const float x0 = std::min(static_cast<float>(x) * field.cellSize, static_cast<float>(field.coverageSize.x));
            const float y0 = std::min(static_cast<float>(y) * field.cellSize, static_cast<float>(field.coverageSize.y));
            const float x1 = std::min(static_cast<float>(x + 1) * field.cellSize, static_cast<float>(field.coverageSize.x));
            const float y1 = std::min(static_cast<float>(y + 1) * field.cellSize, static_cast<float>(field.coverageSize.y));
            if (x1 <= x0 || y1 <= y0) {
                continue;
            }

            const glm::vec4 potentials(
                field.valueAt(x, y),
                field.valueAt(x + 1, y),
                field.valueAt(x, y + 1),
                field.valueAt(x + 1, y + 1)
            );
            if (potentials.x == 0.0f && potentials.y == 0.0f && potentials.z == 0.0f && potentials.w == 0.0f) {
                continue;
            }

            fieldData.push_back(FieldInstance{
                .origin = glm::vec4(x0, y0, field.z, 0.0f),
                .potentials = potentials,
                .cellSize = glm::vec2(x1 - x0, y1 - y0),
            });
            maxValue = std::max(maxValue, std::abs(potentials.x));
            maxValue = std::max(maxValue, std::abs(potentials.y));
            maxValue = std::max(maxValue, std::abs(potentials.z));
            maxValue = std::max(maxValue, std::abs(potentials.w));
        }
    }
    if (fieldData.empty()) {
        return;
    }

    if (gridUniformSlotIndex_ >= kGridUniformSlotCount) {
        gridUniformSlotIndex_ = kGridUniformSlotCount - 1;
    }
    const size_t uniformSlot = gridUniformSlotIndex_++;
    SceneUniforms fieldUniforms = currentSceneUniforms_;
    fieldUniforms.maxCount.x = renderData.fieldAutoScale ? maxValue : std::max(renderData.fieldPotentialScale, 0.0001f);
    fieldUniforms.maxCount.y = std::clamp(renderData.fieldSmoothing, 0.0f, 1.0f);
    WGPUContext::instance().queue()->writeBuffer(*gridUniformBuffers[uniformSlot], 0, &fieldUniforms, sizeof(fieldUniforms));

    const uint64_t instBytes = fieldData.size() * sizeof(FieldInstance);
    if (!potentialFieldInstVb || instBytes > potentialFieldInstVbCapacity_) {
        potentialFieldInstVb =
            WGPUContext::instance().createBuffer(instBytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Vector_Field_Grid_Instances");
        potentialFieldInstVbCapacity_ = instBytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*potentialFieldInstVb, 0, fieldData.data(), instBytes);

    currentPass->setPipeline(*potentialFieldPipeline);
    currentPass->setBindGroup(0, *gridBindGroups[uniformSlot], 0, nullptr);
    currentPass->setVertexBuffer(0, *potentialFieldQuadVb, 0, potentialFieldQuadVb->getSize());
    currentPass->setVertexBuffer(1, *potentialFieldInstVb, 0, instBytes);
    currentPass->draw(6, fieldData.size(), 0, 0);
}

void RendererWGPU::drawFieldArrowsImpl(const RenderData& renderData) {
    const RenderVectorFieldView& field = renderData.vectorField;
    if (field.empty() || field.vectors == nullptr) {
        return;
    }

    const size_t vectorCount = field.vectorCount();
    if (vectorCount == 0) {
        return;
    }

    const uint64_t bytes = vectorCount * sizeof(glm::vec2);
    if (!fieldArrowVb || bytes > fieldArrowVbCapacity_) {
        fieldArrowVb = WGPUContext::instance().createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Field_Arrow_Vectors");
        fieldArrowVbCapacity_ = bytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*fieldArrowVb, 0, field.vectors, bytes);

    if (lineUniformSlotIndex_ >= kLineUniformSlotCount) {
        lineUniformSlotIndex_ = kLineUniformSlotCount - 1;
    }
    const size_t uniformSlot = lineUniformSlotIndex_++;
    SceneUniforms arrowUniforms = currentSceneUniforms_;
    arrowUniforms.lineColor = glm::vec4(1.0f, 0.86f, 0.28f, 0.75f);
    arrowUniforms.maxCount = glm::vec4(static_cast<float>(field.gridSize.x), static_cast<float>(field.gridSize.y), field.cellSize, field.z);
    WGPUContext::instance().queue()->writeBuffer(*lineUniformBuffers[uniformSlot], 0, &arrowUniforms, sizeof(arrowUniforms));

    currentPass->setPipeline(*fieldArrowPipeline);
    currentPass->setBindGroup(0, *lineBindGroups[lineUniformSlotIndex_ - 1], 0, nullptr);
    currentPass->setVertexBuffer(0, *fieldArrowVb, 0, bytes);
    currentPass->draw(6, vectorCount, 0, 0);
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
