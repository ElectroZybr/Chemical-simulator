#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>

#include "Rendering/BaseRenderer.h"

class GpuResidentPhysics; // render-bind seam: резидентные pos/vel в GPU-режиме

class RendererWGPU : public IRenderer {
public:
    RendererWGPU(World& world, wgpu::TextureFormat surfaceFormat);
    ~RendererWGPU() override = default;

    void drawShot(wgpu::TextureView targetView, wgpu::TextureView depthView, const Simulation& simulation) override;
    void endFrame() override;

    wgpu::raii::RenderPassEncoder& getCurrentPass() { return currentPass; }

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
    // Кеш-ключ GPU-bind-group: пересобираем только при расхождении. -1 в gpuGen_
    // (через флаг valid) означает «ещё не собрана».
    bool gpuBindValid_ = false;
    const void* gpuBindPtr_ = nullptr;       // какой GpuResidentPhysics биндили
    uint64_t gpuBindGeneration_ = 0;         // его renderBufferGeneration() на момент сборки
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
    // (gpuResident != nullptr) лениво пере-собирает atomBindGroupGpu_ из резидентных
    // pos/vel (boundCount*16) + renderer-owned type/radius/sel, и возвращает её; в
    // CPU-режиме возвращает обычную atomBindGroup. boundCount нужен для bound size
    // резидентных биндингов (min-clamp на стороне вызывающего).
    wgpu::BindGroup ensureAtomBindGroup(size_t boundCount, const GpuResidentPhysics* gpuResident);

    // Draw
    void drawWorldPass(wgpu::TextureView targetView, wgpu::TextureView depthView, const World& world, wgpu::LoadOp targetLoadOp,
                       bool applySelection, const GpuResidentPhysics* gpuResident);
    void beginPass(wgpu::TextureView targetView, wgpu::TextureView depthView, wgpu::LoadOp targetLoadOp);
    void drawAtomsImpl(const AtomStorage& atoms, bool applySelection, const GpuResidentPhysics* gpuResident);
    void drawBondsImpl(const AtomStorage& atoms, const Bond::List& bonds);
    void drawBoxImpl(const Vec3f& worldSize);
    void drawGridImpl(const SpatialGrid& grid);
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
    Vec3f cachedBoxSize_{-1.0, -1.0, -1.0};

    wgpu::raii::CommandEncoder currentEncoder;
};
