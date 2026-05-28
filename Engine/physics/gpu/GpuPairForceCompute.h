#pragma once

#include <cstddef>
#include <cstdint>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

class AtomStorage;
class NeighborList;
class LJForceField;

// GPU compute backend для LJ pair force. Соответствует Full NL контракту
// CPU-стороны: kernel обходит соседей атома i и пишет только в forces[i].
// Newton 3-й закон держится глобально через то, что NL — full-list (каждая
// пара представлена дважды).
//
// Pipeline и буфера создаются лениво (требует WGPUContext::instance() с
// готовым device). Буфера атомов растут с 1.5x запасом, NL буфера — на
// фактический размер пар, пересоздаются при росте.
class GpuPairForceCompute {
public:
    GpuPairForceCompute();
    ~GpuPairForceCompute();

    GpuPairForceCompute(const GpuPairForceCompute&) = delete;
    GpuPairForceCompute& operator=(const GpuPairForceCompute&) = delete;

    // Один LJ-pass: загрузка позиций / типов / NL → dispatch → readback в
    // atoms.forceX/Y/Z/energy. Накопление через += в существующие значения.
    // Требует, чтобы neighborList был в NeighborListMode::Full.
    void compute(AtomStorage& atoms, const NeighborList& neighborList, const LJForceField& ljForceField);

    // Сигналы кэш-инвалидации: позиции/forces заливаются каждый вызов compute,
    // а NL и type indices живут на GPU между вызовами. Caller должен дёргать
    // эти методы после NL rebuild или после addAtom/removeAtom.
    void invalidateNeighborList() noexcept { nlUploaded_ = false; }
    void invalidateTypes() noexcept { typesUploaded_ = false; }

private:
    void ensureInitialized();
    void uploadLJPairTable(const LJForceField& ljForceField);
    void ensureBufferCapacity(size_t mobileCount, size_t totalCount, size_t neighborCount);
    void uploadInputs(const AtomStorage& atoms, const NeighborList& neighborList);
    void dispatch(uint32_t mobileCount);
    void readbackForces(AtomStorage& atoms);

    bool initialized_ = false;
    bool ljTableUploaded_ = false;

    wgpu::raii::ComputePipeline pipeline_;
    wgpu::raii::BindGroupLayout bindGroupLayout_;
    wgpu::raii::BindGroup bindGroup_;

    wgpu::raii::Buffer uniformBuffer_;
    wgpu::raii::Buffer positionsBuffer_;
    wgpu::raii::Buffer typesBuffer_;
    wgpu::raii::Buffer nlOffsetsBuffer_;
    wgpu::raii::Buffer nlNeighborsBuffer_;
    wgpu::raii::Buffer ljPairsBuffer_;
    wgpu::raii::Buffer forcesBuffer_;
    wgpu::raii::Buffer readbackBuffer_;

    size_t atomCapacity_ = 0;
    size_t nlOffsetsCapacity_ = 0;
    size_t nlNeighborsCapacity_ = 0;
    bool bindGroupValid_ = false;
    bool nlUploaded_ = false;
    bool typesUploaded_ = false;
};
