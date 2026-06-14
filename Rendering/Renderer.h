#pragma once

#include <array>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "Rendering/BaseRenderer.h"
#include "Rendering/layers/LayerState.h"
#include "Rendering/pipelines/PipelineState.h"


class RendererWGPU : public BaseRenderer {
public:
    RendererWGPU();
    ~RendererWGPU() override = default;

    void drawShot(wgpu::TextureView targetView, wgpu::TextureView depthView) override;
    void endFrame() override;
    wgpu::raii::RenderPassEncoder* currentRenderPass() override { return &currentPass; }
    const wgpu::raii::RenderPassEncoder* currentRenderPass() const override { return &currentPass; }

protected:
    virtual void updateMatrices() = 0;
    virtual glm::vec3 getLightDir() = 0;
    virtual bool useLighting() = 0;

    glm::mat4 projection{1.f};
    glm::mat4 view{1.f};

    void initAtomSpherePipeline(std::string_view atomWGSL);
    void initSharedPipelines();
    void initGridPipeline(std::string_view gridWGSL);
    void initPotentialFieldPipeline(std::string_view potentialFieldWGSL);
    void initFieldArrowPipeline(std::string_view fieldArrowWGSL);
    void initFieldContourPipeline(std::string_view fieldContourWGSL);
    void initBoxPipeline(std::string_view boxWGSL);
    void initBondPipeline(std::string_view bondWGSL);
    void initMemoryOrderPipeline(std::string_view memoryOrderWGSL);

private:
    static constexpr size_t kLineUniformSlotCount = 6;
    static constexpr size_t kGridUniformSlotCount = 2;

    struct SceneUniforms {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec4 lightDir;
        glm::vec4 colorMode;
        glm::vec4 maxSpeedSqr;
        glm::vec4 maxCount;
        glm::vec4 renderOffset;
        glm::vec4 lineColor;
        glm::vec4 typeColors[119];
    };
    wgpu::raii::Buffer uniformBuffer;

    PipelineState pipelines_;
    AtomLayerState atomLayer_;
    LineLayerState<kLineUniformSlotCount> lineLayer_;
    GridLayerState<kGridUniformSlotCount> gridLayer_;
    FieldLayerState fieldLayer_;
    wgpu::raii::RenderPassEncoder currentPass;
    wgpu::TextureFormat surfaceFormat;

    void initAtomColors();
    void initAtomQuadBuffer();
    void initBoxBuffer();
    void initBondBuffer();
    void initFieldArrowBuffer();
    void initGridLineBuffer();
    void initPotentialFieldQuadBuffer();
    void initMemoryOrderBuffer();
    void initLinePipeline(wgpu::RenderPipeline& outPipeline, std::string_view wgsl);

    void ensureStorageBuffers(size_t count);
    template <typename T> void uploadStorageBuffer(wgpu::Buffer& buf, const T* data, size_t count);

    void drawWorldPass(wgpu::TextureView targetView, wgpu::TextureView depthView, const RenderData& renderData, wgpu::LoadOp targetLoadOp,
                       bool applySelection);
    void beginPass(wgpu::TextureView targetView, wgpu::TextureView depthView, wgpu::LoadOp targetLoadOp);
    void drawAtomsImpl(const RenderAtomsView& atoms, const RenderData& renderData, bool applySelection);
    void drawBondsImpl(const RenderAtomsView& atoms, const RenderBondsView& bonds);
    void drawBoxImpl(const glm::vec3& worldSize);
    void drawGridImpl(const RenderGridView& grid);
    void drawVectorFieldImpl(const RenderData& renderData);
    void drawFieldArrowsImpl(const RenderData& renderData);
    void drawFieldContoursImpl(const RenderData& renderData);
    void drawMemoryOrderImpl(const RenderAtomsView& atoms);
    void setLineColor(const glm::vec4& color);
    bool rebuildFieldInstances(const RenderVectorFieldView& field, bool skipZeroCells);

    wgpu::raii::CommandEncoder currentEncoder;
    SceneUniforms currentSceneUniforms_{};
    size_t lineUniformSlotIndex_ = 0;
    size_t gridUniformSlotIndex_ = 0;
};
