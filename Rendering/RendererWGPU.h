#pragma once

#include <array>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "Rendering/BaseRenderer.h"

// MERGE-RESOLUTION (zero-copy x upstream render/engine split)
// ----------------------------------------------------------------------------
// Upstream откреплил рендер от движка: RendererWGPU теперь наследует BaseRenderer
// и читает данные атомов ТОЛЬКО через RenderData/RenderAtomsView (struct сырых
// указателей), которые наполняет App-слой (SimulationSceneSource::syncRendererWith
// Simulation). Рендер БОЛЬШЕ НЕ включает Engine/Simulation.
//
// Наш zero-copy (резидентные pos/vel в VRAM у GpuResidentPhysics) встроен НЕ
// прямой связью рендер↔движок, а через РАСШИРЕНИЕ RenderAtomsView: опциональный
// дескриптор RenderAtomsGpuResidency (см. RenderData.h) несёт два wgpu::Buffer
// (pos/vel), generation и boundCount. Его наполняет тот же App-слой из
// simulation.gpuResidentAt(worldId) — единственная точка, которой и положено знать
// движок. Рендер зависит только от wgpu-типов, которые уже использует; #include
// "GpuResidentPhysics.h" из Rendering УБРАН. Так контракт «рендер не зависит от
// движка/компонентов» сохранён, а per-frame download убран ровно как в нашей ветке.
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
    void initBoxPipeline(std::string_view boxWGSL);
    void initBondPipeline(std::string_view bondWGSL);

private:
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

    // Bind group layouts
    wgpu::raii::BindGroupLayout atomBindGroupLayout;
    wgpu::raii::BindGroupLayout lineBindGroupLayout;
    wgpu::raii::BindGroupLayout gridBindGroupLayout;
    wgpu::raii::BindGroup lineBindGroup;
    wgpu::raii::BindGroup gridBindGroup;

    // Vertex buffers
    wgpu::raii::Buffer atomQuadVb;
    wgpu::raii::Buffer bondVb;
    wgpu::raii::Buffer boxVb;
    wgpu::raii::Buffer gridLineVb;
    wgpu::raii::Buffer gridInstVb;

    // Storage buffers
    wgpu::raii::Buffer sbPos;    // array<vec4<f32>> — x,y,z,pad
    wgpu::raii::Buffer sbVel;    // array<vec4<f32>> — vx,vy,vz,pad
    wgpu::raii::Buffer sbType;   // array<f32>
    wgpu::raii::Buffer sbRadius; // array<f32>
    wgpu::raii::Buffer sbSel;    // array<f32>

    size_t sbCapacity_ = 0;
    size_t bondVbCapacity_ = 0;
    size_t gridInstVbCapacity_ = 0;

    wgpu::raii::BindGroup atomBindGroup;

    // Zero-copy GPU-режим: отдельная atom-bind-group, биндящая РЕЗИДЕНТНЫЕ pos/vel
    // физики (binding 1/2) + renderer-owned type/radius/sel (binding 3/4/5). Layout
    // тот же (atomBindGroupLayout), меняются только два буфера. Пере-собирается
    // лениво по кеш-ключу ниже — на статичной сцене горячий путь её не трогает.
    wgpu::raii::BindGroup atomBindGroupGpu_;
    // Кеш-ключ GPU-bind-group: пересобираем только при расхождении. valid==false
    // означает «ещё не собрана». Раньше ключ хранил GpuResidentPhysics* (рендер знал
    // движок); теперь — wgpu::Buffer pos (как стабильный идентификатор резидентного
    // источника) + generation, что не требует знания типа движка.
    bool gpuBindValid_ = false;
    WGPUBuffer gpuBindPosBuffer_ = nullptr;  // какой резидентный pos-буфер биндили (идентичность источника)
    uint64_t gpuBindGeneration_ = 0;         // его generation на момент сборки
    size_t gpuBindSbCapacity_ = 0;           // ёмкость renderer-owned sbType/sbRadius/sbSel
    size_t gpuBindBoundCount_ = 0;           // boundCount, под который выставлен bound size резидентных pos/vel

    wgpu::raii::RenderPassEncoder currentPass;

    wgpu::TextureFormat surfaceFormat;

    void initAtomColors();
    void initAtomQuadBuffer();
    void initBoxBuffer();
    void initBondBuffer();
    void initGridLineBuffer();
    void initLinePipeline(wgpu::RenderPipeline& outPipeline, std::string_view wgsl);

    // Helpers
    void ensureStorageBuffers(size_t count);
    template <typename T> void uploadStorageBuffer(wgpu::Buffer& buf, const T* data, size_t count);
    // Гарантирует валидную atom-bind-group для текущего режима. В GPU-режиме
    // (residency.valid) лениво пере-собирает atomBindGroupGpu_ из резидентных
    // pos/vel (boundCount*16) + renderer-owned type/radius/sel и возвращает её; в
    // CPU-режиме возвращает обычную atomBindGroup. boundCount нужен для bound size
    // резидентных биндингов (min-clamp на стороне вызывающего).
    wgpu::BindGroup ensureAtomBindGroup(size_t boundCount, const RenderAtomsGpuResidency& residency);

    // Draw
    void drawWorldPass(wgpu::TextureView targetView, wgpu::TextureView depthView, const RenderData& renderData, wgpu::LoadOp targetLoadOp,
                       bool applySelection);
    void beginPass(wgpu::TextureView targetView, wgpu::TextureView depthView, wgpu::LoadOp targetLoadOp);
    void drawAtomsImpl(const RenderAtomsView& atoms, const RenderData& renderData, bool applySelection);
    void drawBondsImpl(const RenderAtomsView& atoms, const RenderBondsView& bonds);
    void drawBoxImpl(const glm::vec3& worldSize);
    void drawGridImpl(const RenderGridView& grid);
    void setLineColor(const glm::vec4& color);

    // Data
    struct GridInstance {
        glm::vec4 origin;
        float cellSize;
        float atomCount;
        float pad[2] = {};
    };

    struct AtomVec4 {
        float x, y, z, pad = 0.f;
    };
    std::vector<AtomVec4> posData_;
    std::vector<AtomVec4> velData_;

    std::vector<GridInstance> gridData;
    std::vector<glm::vec4> typeColorsData;
    std::vector<float> selectedData;
    std::vector<float> radii;
    std::vector<float> typeData;
    std::array<float, 24 * 3> boxVertices_{};
    glm::vec3 cachedBoxSize_{-1.0f, -1.0f, -1.0f};

    wgpu::raii::CommandEncoder currentEncoder;
};
