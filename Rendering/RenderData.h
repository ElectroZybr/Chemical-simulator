#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include <webgpu/webgpu.hpp>

struct RenderColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

// MERGE-ADD (zero-copy x upstream render/engine split)
// ----------------------------------------------------------------------------
// Опциональный дескриптор РЕЗИДЕНТНЫХ pos/vel атомов в VRAM. Это и есть мост,
// которым наш zero-copy примирён с upstream-контрактом «рендер не зависит от
// движка»: рендер по-прежнему видит только Rendering-типы (wgpu::Buffer), а
// откуда взялись буфера (GpuResidentPhysics) знает ТОЛЬКО App-слой, который и
// наполняет это поле в SimulationSceneSource::syncRendererWithSimulation из
// simulation.gpuResidentAt(worldId).
//
// valid == false  -> CPU-режим: рендер пакует/заливает pos/vel из RenderAtomsView::x/y/z.
// valid == true   -> GPU-режим: рендер биндит positions/velocities zero-copy,
//                    рисует ровно boundCount атомов, пере-биндит при смене generation.
//
// Контракт буферов (диктуется GpuResidentPhysics, см. его renderBufferGeneration/
// renderBoundCount/positionsBuffer): формат array<vec4<f32>> (x,y,z,pad), bound size
// биндинга = boundCount*16 байт; generation МЕНЯЕТСЯ при пересоздании буфера (рост
// сцены) — рендер обязан тогда пере-биндить (кеш-ключ это учитывает).
struct RenderAtomsGpuResidency {
    bool valid = false;
    WGPUBuffer positions = nullptr;  // резидентные позиции (array<vec4<f32>>)
    WGPUBuffer velocities = nullptr; // резидентные скорости (array<vec4<f32>>)
    uint64_t generation = 0;         // GpuResidentPhysics::renderBufferGeneration() на момент снимка
    size_t boundCount = 0;           // GpuResidentPhysics::renderBoundCount() — сколько атомов в VRAM
};

struct RenderAtomsView {
    size_t count = 0;

    const float* x = nullptr;
    const float* y = nullptr;
    const float* z = nullptr;

    const float* vx = nullptr;
    const float* vy = nullptr;
    const float* vz = nullptr;

    const uint8_t* type = nullptr;
    const float* radius = nullptr;

    // Резидентные VRAM-буфера (zero-copy). По умолчанию invalid -> CPU-путь.
    RenderAtomsGpuResidency gpu{};

    [[nodiscard]] bool empty() const noexcept { return count == 0; }
    [[nodiscard]] bool hasPositions() const noexcept { return x != nullptr && y != nullptr && z != nullptr; }
    [[nodiscard]] bool hasVelocities() const noexcept { return vx != nullptr && vy != nullptr && vz != nullptr; }
    [[nodiscard]] bool hasTypes() const noexcept { return type != nullptr; }
    [[nodiscard]] bool hasRadii() const noexcept { return radius != nullptr; }
};

struct RenderBond {
    size_t aIndex = 0;
    size_t bIndex = 0;
};

struct RenderGridCell {
    glm::vec3 origin{};
    float cellSize = 1.0f;
    float atomCount = 0.0f;
};

using RenderBondVisitor = void (*)(size_t aIndex, size_t bIndex, void* userData);
using RenderGridCellVisitor = void (*)(const RenderGridCell& cell, void* userData);

struct RenderBondsView {
    const void* context = nullptr;
    size_t count = 0;
    void (*forEachFn)(const void* context, RenderBondVisitor visitor, void* userData) = nullptr;

    [[nodiscard]] bool empty() const noexcept { return count == 0 || context == nullptr || forEachFn == nullptr; }
    void forEach(RenderBondVisitor visitor, void* userData) const {
        if (!empty()) {
            forEachFn(context, visitor, userData);
        }
    }
};

struct RenderGridView {
    const void* context = nullptr;
    size_t count = 0;
    void (*forEachFn)(const void* context, RenderGridCellVisitor visitor, void* userData) = nullptr;

    [[nodiscard]] bool empty() const noexcept { return count == 0 || context == nullptr || forEachFn == nullptr; }
    void forEach(RenderGridCellVisitor visitor, void* userData) const {
        if (!empty()) {
            forEachFn(context, visitor, userData);
        }
    }
};

class RenderData {
public:
    enum class SpeedColorMode : uint8_t {
        AtomColor = 0,
        GradientClassic = 1,
        GradientTurbo = 2,
    };

    RenderAtomsView atoms{};

    RenderBondsView bonds{};
    RenderGridView grid{};
    std::vector<size_t> selectedAtomIndices;

    glm::vec3 worldSize{0.0f, 0.0f, 0.0f};
    glm::vec3 renderOffset{0.0f, 0.0f, 0.0f};

    bool isActiveWorld = false;
    bool hasBox = false;
    bool drawGrid = false;
    bool drawBonds = false;
    bool drawBox = true;
    SpeedColorMode speedColorMode = SpeedColorMode::AtomColor;
    float speedGradientMax = 5.0f;
    float alpha = 0.05f;
};
