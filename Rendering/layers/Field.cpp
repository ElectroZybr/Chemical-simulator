#include <algorithm>
#include <vector>

#include "Rendering/Renderer.h"
#include "Rendering/backend/WGPUContext.h"

namespace {
    float resolvePotentialScale(const RenderData& renderData, const RenderVectorFieldView& field) {
        if (!renderData.fieldAutoScale) {
            return std::max(renderData.fieldPotentialScale, 0.0001f);
        }

        float maxValue = 1.0f;
        for (int y = 0; y < field.gridSize.y; ++y) {
            for (int x = 0; x < field.gridSize.x; ++x) {
                maxValue = std::max(maxValue, std::abs(field.valueAt(x, y)));
            }
        }
        return maxValue;
    }

    float resolveContourScale(const RenderData& renderData, const RenderVectorFieldView& field) {
        float contourScale = std::max(renderData.fieldPotentialScale, 0.0001f);
        if (!renderData.fieldAutoScale) {
            return contourScale;
        }

        std::vector<float> absoluteValues;
        absoluteValues.reserve(static_cast<size_t>(field.gridSize.x) * static_cast<size_t>(field.gridSize.y));
        for (int y = 0; y < field.gridSize.y; ++y) {
            for (int x = 0; x < field.gridSize.x; ++x) {
                absoluteValues.push_back(std::abs(field.valueAt(x, y)));
            }
        }

        if (absoluteValues.empty()) {
            return contourScale;
        }

        const size_t percentileIndex = std::min(absoluteValues.size() - 1, (absoluteValues.size() * 9) / 10);
        std::nth_element(absoluteValues.begin(), absoluteValues.begin() + static_cast<std::ptrdiff_t>(percentileIndex), absoluteValues.end());
        return std::max(absoluteValues[percentileIndex], 0.0001f);
    }

}

bool RendererWGPU::rebuildFieldInstances(const RenderVectorFieldView& field, bool skipZeroCells) {
    fieldLayer_.fieldData.clear();
    fieldLayer_.fieldData.reserve(field.cellCount());
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
            if (skipZeroCells && potentials.x == 0.0f && potentials.y == 0.0f && potentials.z == 0.0f && potentials.w == 0.0f) {
                continue;
            }

            fieldLayer_.fieldData.push_back(FieldInstance{
                .origin = glm::vec4(x0, y0, field.z, 0.0f),
                .potentials = potentials,
                .cellSize = glm::vec2(x1 - x0, y1 - y0),
            });
        }
    }

    return !fieldLayer_.fieldData.empty();
}

void RendererWGPU::initPotentialFieldQuadBuffer() {
    static constexpr float quad[] = {
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
    };
    WGPUContext& ctx = WGPUContext::instance();
    fieldLayer_.potentialFieldQuadVb = ctx.createBuffer(sizeof(quad), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Potential_Field_Quad");
    ctx.queue()->writeBuffer(*fieldLayer_.potentialFieldQuadVb, 0, quad, sizeof(quad));
}

void RendererWGPU::drawVectorFieldImpl(const RenderData& renderData) {
    const RenderVectorFieldView& field = renderData.vectorField;
    if (field.empty()) {
        return;
    }

    if (!rebuildFieldInstances(field, true)) {
        return;
    }

    if (gridUniformSlotIndex_ >= RendererWGPU::kGridUniformSlotCount) {
        gridUniformSlotIndex_ = RendererWGPU::kGridUniformSlotCount - 1;
    }
    const size_t uniformSlot = gridUniformSlotIndex_++;
    RendererWGPU::SceneUniforms fieldUniforms = currentSceneUniforms_;
    fieldUniforms.maxCount.x = resolvePotentialScale(renderData, field);
    fieldUniforms.maxCount.y = std::clamp(renderData.fieldSmoothing, 0.0f, 1.0f);
    WGPUContext::instance().queue()->writeBuffer(*gridLayer_.uniformBuffers[uniformSlot], 0, &fieldUniforms, sizeof(fieldUniforms));

    const uint64_t instBytes = fieldLayer_.fieldData.size() * sizeof(FieldInstance);
    if (!fieldLayer_.potentialFieldInstVb || instBytes > fieldLayer_.potentialFieldInstVbCapacity) {
        fieldLayer_.potentialFieldInstVb =
            WGPUContext::instance().createBuffer(instBytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Vector_Field_Grid_Instances");
        fieldLayer_.potentialFieldInstVbCapacity = instBytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*fieldLayer_.potentialFieldInstVb, 0, fieldLayer_.fieldData.data(), instBytes);

    currentPass->setPipeline(*pipelines_.potentialField);
    currentPass->setBindGroup(0, *gridLayer_.bindGroups[uniformSlot], 0, nullptr);
    currentPass->setVertexBuffer(0, *fieldLayer_.potentialFieldQuadVb, 0, fieldLayer_.potentialFieldQuadVb->getSize());
    currentPass->setVertexBuffer(1, *fieldLayer_.potentialFieldInstVb, 0, instBytes);
    currentPass->draw(6, fieldLayer_.fieldData.size(), 0, 0);
}

void RendererWGPU::drawFieldContoursImpl(const RenderData& renderData) {
    const RenderVectorFieldView& field = renderData.vectorField;
    if (field.empty()) {
        return;
    }

    if (!rebuildFieldInstances(field, false)) {
        return;
    }

    if (gridUniformSlotIndex_ >= RendererWGPU::kGridUniformSlotCount) {
        gridUniformSlotIndex_ = RendererWGPU::kGridUniformSlotCount - 1;
    }
    const size_t uniformSlot = gridUniformSlotIndex_++;
    RendererWGPU::SceneUniforms contourUniforms = currentSceneUniforms_;
    contourUniforms.maxCount.x = resolveContourScale(renderData, field);
    contourUniforms.maxCount.y = std::clamp(renderData.fieldSmoothing, 0.0f, 1.0f);
    contourUniforms.maxCount.z = std::clamp(renderData.fieldContourStep, 0.01f, 1.0f);
    contourUniforms.lineColor = glm::vec4(0.96f, 0.96f, 0.96f, 0.92f);
    WGPUContext::instance().queue()->writeBuffer(*gridLayer_.uniformBuffers[uniformSlot], 0, &contourUniforms, sizeof(contourUniforms));

    const uint64_t instBytes = fieldLayer_.fieldData.size() * sizeof(FieldInstance);
    if (!fieldLayer_.potentialFieldInstVb || instBytes > fieldLayer_.potentialFieldInstVbCapacity) {
        fieldLayer_.potentialFieldInstVb =
            WGPUContext::instance().createBuffer(instBytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Vector_Field_Grid_Instances");
        fieldLayer_.potentialFieldInstVbCapacity = instBytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*fieldLayer_.potentialFieldInstVb, 0, fieldLayer_.fieldData.data(), instBytes);

    currentPass->setPipeline(*pipelines_.fieldContours);
    currentPass->setBindGroup(0, *gridLayer_.bindGroups[uniformSlot], 0, nullptr);
    currentPass->setVertexBuffer(0, *fieldLayer_.potentialFieldQuadVb, 0, fieldLayer_.potentialFieldQuadVb->getSize());
    currentPass->setVertexBuffer(1, *fieldLayer_.potentialFieldInstVb, 0, instBytes);
    currentPass->draw(6, fieldLayer_.fieldData.size(), 0, 0);
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
    if (!fieldLayer_.fieldArrowVb || bytes > fieldLayer_.fieldArrowVbCapacity) {
        fieldLayer_.fieldArrowVb =
            WGPUContext::instance().createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Field_Arrow_Vectors");
        fieldLayer_.fieldArrowVbCapacity = bytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*fieldLayer_.fieldArrowVb, 0, field.vectors, bytes);

    if (lineUniformSlotIndex_ >= RendererWGPU::kLineUniformSlotCount) {
        lineUniformSlotIndex_ = RendererWGPU::kLineUniformSlotCount - 1;
    }
    const size_t uniformSlot = lineUniformSlotIndex_++;
    RendererWGPU::SceneUniforms arrowUniforms = currentSceneUniforms_;
    arrowUniforms.lineColor = glm::vec4(1.0f, 0.86f, 0.28f, 0.75f);
    arrowUniforms.maxCount = glm::vec4(static_cast<float>(field.gridSize.x), static_cast<float>(field.gridSize.y), field.cellSize, field.z);
    WGPUContext::instance().queue()->writeBuffer(*lineLayer_.uniformBuffers[uniformSlot], 0, &arrowUniforms, sizeof(arrowUniforms));

    currentPass->setPipeline(*pipelines_.fieldArrows);
    currentPass->setBindGroup(0, *lineLayer_.bindGroups[uniformSlot], 0, nullptr);
    currentPass->setVertexBuffer(0, *fieldLayer_.fieldArrowVb, 0, bytes);
    currentPass->draw(6, vectorCount, 0, 0);
}
