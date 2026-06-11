#pragma once

#include <array>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "Rendering/BaseRenderer.h"

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

    void initAtomPipeline(std::string_view atomWGSL);
    void initGridPipeline(std::string_view gridWGSL);
    void initPotentialFieldPipeline(std::string_view potentialFieldWGSL);
    void initFieldArrowPipeline(std::string_view fieldArrowWGSL);
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
        glm::vec4 colorMode;   // x = SpeedColorMode
        glm::vec4 maxSpeedSqr; // x = value
        glm::vec4 maxCount;    // x = value
        glm::vec4 renderOffset;
        glm::vec4 lineColor;
        glm::vec4 typeColors[119];
    };
    wgpu::raii::Buffer uniformBuffer;

    // Pipelines (аналог bgfx::ProgramHandle)
    wgpu::raii::RenderPipeline atomPipeline;
    wgpu::raii::RenderPipeline bondPipeline;
    wgpu::raii::RenderPipeline boxPipeline;
    wgpu::raii::RenderPipeline gridPipeline;
    wgpu::raii::RenderPipeline potentialFieldPipeline;
    wgpu::raii::RenderPipeline fieldArrowPipeline;
    wgpu::raii::RenderPipeline memoryOrderPipeline;

    // Bind group layouts
    wgpu::raii::BindGroupLayout atomBindGroupLayout;
    wgpu::raii::BindGroupLayout lineBindGroupLayout;
    wgpu::raii::BindGroupLayout gridBindGroupLayout;
    std::array<wgpu::raii::Buffer, kLineUniformSlotCount> lineUniformBuffers;
    std::array<wgpu::raii::BindGroup, kLineUniformSlotCount> lineBindGroups;
    std::array<wgpu::raii::Buffer, kGridUniformSlotCount> gridUniformBuffers;
    std::array<wgpu::raii::BindGroup, kGridUniformSlotCount> gridBindGroups;

    // Vertex buffers
    wgpu::raii::Buffer atomQuadVb;
    wgpu::raii::Buffer bondVb;
    wgpu::raii::Buffer fieldArrowVb;
    wgpu::raii::Buffer boxVb;
    wgpu::raii::Buffer gridLineVb;
    wgpu::raii::Buffer potentialFieldQuadVb;
    wgpu::raii::Buffer gridInstVb;
    wgpu::raii::Buffer potentialFieldInstVb;
    wgpu::raii::Buffer memoryOrderVb;  // для отрисовки линий между атомами в той же последовательности в которой они леат в памяти

    // Storage buffers
    wgpu::raii::Buffer sbPos;    // array<vec4<f32>> — x,y,z,pad
    wgpu::raii::Buffer sbVel;    // array<vec4<f32>> — vx,vy,vz,pad
    wgpu::raii::Buffer sbType;   // array<f32>
    wgpu::raii::Buffer sbRadius; // array<f32>
    wgpu::raii::Buffer sbSel;    // array<f32>

    size_t sbCapacity_ = 0;
    size_t bondVbCapacity_ = 0;
    size_t fieldArrowVbCapacity_ = 0;
    size_t gridInstVbCapacity_ = 0;
    size_t potentialFieldInstVbCapacity_ = 0;
    size_t memoryOrderVbCapacity_ = 0;

    wgpu::raii::BindGroup atomBindGroup;

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

    // Helpers
    void ensureStorageBuffers(size_t count);
    template <typename T> void uploadStorageBuffer(wgpu::Buffer& buf, const T* data, size_t count);

    // Draw
    void drawWorldPass(wgpu::TextureView targetView, wgpu::TextureView depthView, const RenderData& renderData, wgpu::LoadOp targetLoadOp,
                       bool applySelection);
    void beginPass(wgpu::TextureView targetView, wgpu::TextureView depthView, wgpu::LoadOp targetLoadOp);
    void drawAtomsImpl(const RenderAtomsView& atoms, const RenderData& renderData, bool applySelection);
    void drawBondsImpl(const RenderAtomsView& atoms, const RenderBondsView& bonds);
    void drawBoxImpl(const glm::vec3& worldSize);
    void drawGridImpl(const RenderGridView& grid);
    void drawVectorFieldImpl(const RenderData& renderData);
    void drawFieldArrowsImpl(const RenderData& renderData);
    void drawMemoryOrderImpl(const RenderAtomsView& atoms);
    void setLineColor(const glm::vec4& color);

    // Data
    struct GridInstance {
        glm::vec4 origin;
        float cellSize;
        float atomCount;
        float pad[2] = {};
    };

    struct FieldInstance {
        glm::vec4 origin;
        glm::vec4 potentials;
        glm::vec2 cellSize;
        float pad[2] = {};
    };

    struct AtomVec4 {
        float x, y, z, pad = 0.f;
    };
    struct MemoryOrderVertex {
        glm::vec3 pos;
        float t = 0.0f;
    };
    std::vector<AtomVec4> posData_;
    std::vector<AtomVec4> velData_;

    std::vector<GridInstance> gridData;
    std::vector<FieldInstance> fieldData;
    std::vector<glm::vec4> typeColorsData;
    std::vector<float> selectedData;
    std::vector<float> radii;
    std::vector<float> typeData;
    std::array<float, 24 * 3> boxVertices_{};
    glm::vec3 cachedBoxSize_{-1.0f, -1.0f, -1.0f};

    wgpu::raii::CommandEncoder currentEncoder;
    SceneUniforms currentSceneUniforms_{};
    size_t lineUniformSlotIndex_ = 0;
    size_t gridUniformSlotIndex_ = 0;
};
