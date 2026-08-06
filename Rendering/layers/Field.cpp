// Modified by AI: optimize/full-opt-ai-1
// This file contains CPU-side optimizations for field preparation.
// Changes: parallel two-phase rebuildFieldInstances using TBB when available,
// parallel reduction for potential scale, sampling for contour percentile.

#include <algorithm>
#include <vector>
#include <mutex>

#include "Rendering/Renderer.h"
#include "Rendering/backend/WGPUContext.h"

#ifdef ENABLE_TBB
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#endif

namespace {
    // Compute maximum absolute value across the field (used for autoscaling).
    float resolvePotentialScaleImpl(const RenderData& renderData, const View::RenderVectorFieldView& field) {
        if (!renderData.fieldAutoScale) {
            return std::max(renderData.fieldPotentialScale, 0.0001f);
        }

        float maxValue = 1.0f;
#ifdef ENABLE_TBB
        std::mutex mtx;
        const int rows = field.gridSize.y;
        tbb::parallel_for(0, rows, [&](int y) {
            float localMax = 0.0f;
            for (int x = 0; x < field.gridSize.x; ++x) {
                localMax = std::max(localMax, std::abs(field.valueAt(x, y)));
            }
            std::lock_guard<std::mutex> lock(mtx);
            maxValue = std::max(maxValue, localMax);
        });
#else
        for (int y = 0; y < field.gridSize.y; ++y) {
            for (int x = 0; x < field.gridSize.x; ++x) {
                maxValue = std::max(maxValue, std::abs(field.valueAt(x, y)));
            }
        }
#endif
        return maxValue;
    }

    // Compute 90th percentile (approx) of absolute values for contours. For very large
    // fields we sample to limit memory and time used by nth_element.
    float resolveContourScaleImpl(const RenderData& renderData, const View::RenderVectorFieldView& field) {
        float contourScale = std::max(renderData.fieldPotentialScale, 0.0001f);
        if (!renderData.fieldAutoScale) {
            return contourScale;
        }

        const size_t total = static_cast<size_t>(field.gridSize.x) * static_cast<size_t>(field.gridSize.y);
        const size_t kMaxSamples = 100000; // tuning parameter
        if (total <= kMaxSamples) {
            std::vector<float> absoluteValues;
            absoluteValues.reserve(total);
            for (int y = 0; y < field.gridSize.y; ++y) {
                for (int x = 0; x < field.gridSize.x; ++x) {
                    absoluteValues.push_back(std::abs(field.valueAt(x, y)));
                }
            }
            if (absoluteValues.empty()) return contourScale;
            const size_t percentileIndex = std::min(absoluteValues.size() - 1, (absoluteValues.size() * 9) / 10);
            std::nth_element(absoluteValues.begin(), absoluteValues.begin() + static_cast<std::ptrdiff_t>(percentileIndex), absoluteValues.end());
            return std::max(absoluteValues[percentileIndex], 0.0001f);
        } else {
            // Uniform sampling to approximate percentile
            const size_t stride = std::max<size_t>(1, total / kMaxSamples);
            std::vector<float> samples;
            samples.reserve(kMaxSamples);
            size_t index = 0;
            for (int y = 0; y < field.gridSize.y; ++y) {
                for (int x = 0; x < field.gridSize.x; ++x, ++index) {
                    if ((index % stride) == 0) samples.push_back(std::abs(field.valueAt(x, y)));
                }
            }
            if (samples.empty()) return contourScale;
            const size_t percentileIndex = std::min(samples.size() - 1, (samples.size() * 9) / 10);
            std::nth_element(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(percentileIndex), samples.end());
            return std::max(samples[percentileIndex], 0.0001f);
        }
    }

}

float RendererWGPU::resolveFieldPotentialScale(const RenderData& renderData, const View::RenderVectorFieldView& field) {
    return resolvePotentialScaleImpl(renderData, field);
}

float RendererWGPU::resolveFieldContourScale(const RenderData& renderData, const View::RenderVectorFieldView& field) {
    return resolveContourScaleImpl(renderData, field);
}

bool RendererWGPU::prepareFieldPotentialCpuData(const RenderData& renderData) {
    const View::RenderVectorFieldView& field = renderData.vectorField;
    if (field.empty()) {
        return false;
    }

    fieldLayer_.preparedScaleX = resolveFieldPotentialScale(renderData, field);
    fieldLayer_.preparedScaleY = glm::clamp(renderData.fieldSmoothing, 0.0f, 1.0f);
    fieldLayer_.preparedScaleZ = 0.0f;
    return rebuildFieldInstances(field, true);
}

bool RendererWGPU::prepareFieldContoursCpuData(const RenderData& renderData) {
    const View::RenderVectorFieldView& field = renderData.vectorField;
    if (field.empty()) {
        return false;
    }

    fieldLayer_.preparedScaleX = resolveFieldContourScale(renderData, field);
    fieldLayer_.preparedScaleY = glm::clamp(renderData.fieldSmoothing, 0.0f, 1.0f);
    fieldLayer_.preparedScaleZ = glm::clamp(renderData.fieldContourStep, 0.01f, 1.0f);
    return rebuildFieldInstances(field, false);
}

bool RendererWGPU::prepareFieldArrowsCpuData(const RenderData& renderData) {
    const View::RenderVectorFieldView& field = renderData.vectorField;
    if (field.empty() || field.vectors == nullptr) {
        fieldLayer_.preparedVectorCount = 0;
        return false;
    }

    fieldLayer_.preparedVectorCount = field.vectorCount();
    return fieldLayer_.preparedVectorCount > 0;
}

bool RendererWGPU::rebuildFieldInstances(const View::RenderVectorFieldView& field, bool skipZeroCells) {
    // Modified by AI: optimize/full-opt-ai-1
    // Two-phase parallel algorithm (uses TBB if ENABLE_TBB):
    // 1) count valid cells per row in parallel
    // 2) prefix-sum to compute write offsets
    // 3) fill preallocated vector in parallel per-row

    const int rows = field.gridSize.y - 1;
    const int cols = field.gridSize.x - 1;
    if (rows <= 0 || cols <= 0) {
        fieldLayer_.fieldData.clear();
        return false;
    }

#ifdef ENABLE_TBB
    std::vector<size_t> rowCounts(static_cast<size_t>(rows), 0);
    tbb::parallel_for(0, rows, [&](int y) {
        size_t localCount = 0;
        for (int x = 0; x < cols; ++x) {
            const float x0 = std::min(static_cast<float>(x) * field.cellSize, static_cast<float>(field.coverageSize.x));
            const float y0 = std::min(static_cast<float>(y) * field.cellSize, static_cast<float>(field.coverageSize.y));
            const float x1 = std::min(static_cast<float>(x + 1) * field.cellSize, static_cast<float>(field.coverageSize.x));
            const float y1 = std::min(static_cast<float>(y + 1) * field.cellSize, static_cast<float>(field.coverageSize.y));
            if (x1 <= x0 || y1 <= y0) continue;
            const float v0 = field.valueAt(x, y);
            const float v1 = field.valueAt(x + 1, y);
            const float v2 = field.valueAt(x, y + 1);
            const float v3 = field.valueAt(x + 1, y + 1);
            if (skipZeroCells && v0 == 0.0f && v1 == 0.0f && v2 == 0.0f && v3 == 0.0f) continue;
            ++localCount;
        }
        rowCounts[static_cast<size_t>(y)] = localCount;
    });

    // prefix sum to compute offsets
    size_t total = 0;
    for (size_t i = 0; i < rowCounts.size(); ++i) {
        const size_t c = rowCounts[i];
        rowCounts[i] = total;
        total += c;
    }

    if (total == 0) {
        fieldLayer_.fieldData.clear();
        return false;
    }

    fieldLayer_.fieldData.clear();
    fieldLayer_.fieldData.resize(total);

    // fill in parallel
    tbb::parallel_for(0, rows, [&](int y) {
        size_t writeIndex = rowCounts[static_cast<size_t>(y)];
        for (int x = 0; x < cols; ++x) {
            const float x0 = std::min(static_cast<float>(x) * field.cellSize, static_cast<float>(field.coverageSize.x));
            const float y0 = std::min(static_cast<float>(y) * field.cellSize, static_cast<float>(field.coverageSize.y));
            const float x1 = std::min(static_cast<float>(x + 1) * field.cellSize, static_cast<float>(field.coverageSize.x));
            const float y1 = std::min(static_cast<float>(y + 1) * field.cellSize, static_cast<float>(field.coverageSize.y));
            if (x1 <= x0 || y1 <= y0) continue;
            const glm::vec4 potentials(
                field.valueAt(x, y),
                field.valueAt(x + 1, y),
                field.valueAt(x, y + 1),
                field.valueAt(x + 1, y + 1)
            );
            if (skipZeroCells && potentials.x == 0.0f && potentials.y == 0.0f && potentials.z == 0.0f && potentials.w == 0.0f) {
                continue;
            }
            fieldLayer_.fieldData[writeIndex++] = FieldInstance{
                .origin = glm::vec4(x0, y0, field.z, 0.0f),
                .potentials = potentials,
                .cellSize = glm::vec2(x1 - x0, y1 - y0),
            };
        }
    });

    return !fieldLayer_.fieldData.empty();
#else
    // fallback sequential implementation
    fieldLayer_.fieldData.clear();
    fieldLayer_.fieldData.reserve(static_cast<size_t>(rows) * static_cast<size_t>(cols));
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
#endif
}
