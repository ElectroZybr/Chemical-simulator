#include "GpuPairForceCompute.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <webgpu/webgpu-raii.hpp>

#include "Engine/Consts.h"
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Rendering/WGPUContext.h"

#include "generated/shaders/physics_lj.wgsl.h"

namespace {

struct ComputeUniforms {
    float cutoffSqr;
    float epsilon;
    uint32_t mobileCount;
    uint32_t typeCount;
};

constexpr size_t kAtomCapacityHeadroom(size_t n) { return n + n / 2 + 1; }

wgpu::ShaderModule makeShaderModule(std::string_view wgsl) {
    WGPUShaderSourceWGSL wgslDesc{};
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = wgpu::StringView(wgsl);

    wgpu::ShaderModuleDescriptor desc{};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
    return WGPUContext::instance().device()->createShaderModule(desc);
}

} // namespace

GpuPairForceCompute::GpuPairForceCompute() = default;
GpuPairForceCompute::~GpuPairForceCompute() = default;

void GpuPairForceCompute::ensureInitialized() {
    if (initialized_) {
        return;
    }

    wgpu::Device device = *WGPUContext::instance().device();
    if (device == nullptr) {
        throw std::runtime_error("GpuPairForceCompute: WGPUContext device not initialized");
    }

    // Bind group layout: uniform + 6 storage buffers (5 read, 1 read_write).
    std::array<wgpu::BindGroupLayoutEntry, 7> entries{};
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Compute;
    entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    for (uint32_t i = 1; i <= 5; ++i) {
        entries[i].binding = i;
        entries[i].visibility = wgpu::ShaderStage::Compute;
        entries[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    }
    entries[6].binding = 6;
    entries[6].visibility = wgpu::ShaderStage::Compute;
    entries[6].buffer.type = wgpu::BufferBindingType::Storage;

    bindGroupLayout_ = WGPUContext::instance().createBindGroupLayout(entries, "GpuPairForce_BGL");

    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView("GpuPairForce_PipelineLayout");
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&bindGroupLayout_);
    wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(plDesc);

    wgpu::ShaderModule shader = makeShaderModule(physics_ljWGSL);

    wgpu::ComputePipelineDescriptor pDesc{};
    pDesc.label = wgpu::StringView("GpuPairForce_Pipeline");
    pDesc.layout = pipelineLayout;
    pDesc.compute.module = shader;
    pDesc.compute.entryPoint = wgpu::StringView("compute_lj");

    pipeline_ = device.createComputePipeline(pDesc);

    uniformBuffer_ = WGPUContext::instance().createBuffer(sizeof(ComputeUniforms),
                                                          wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                          "GpuPairForce_Uniforms");

    initialized_ = true;
}

void GpuPairForceCompute::uploadLJPairTable(const LJForceField& ljForceField) {
    if (ljTableUploaded_) {
        return;
    }

    constexpr size_t kTypeCount = static_cast<size_t>(AtomData::Type::COUNT);
    // Plain vec2<f32>(C6, C12) row-major, total kTypeCount*kTypeCount entries.
    std::vector<float> packed(kTypeCount * kTypeCount * 2);
    for (size_t i = 0; i < kTypeCount; ++i) {
        const auto& row = ljForceField.pairRow(static_cast<AtomData::Type>(i));
        for (size_t j = 0; j < kTypeCount; ++j) {
            packed[(i * kTypeCount + j) * 2 + 0] = row[j].potentialC6;
            packed[(i * kTypeCount + j) * 2 + 1] = row[j].potentialC12;
        }
    }

    const uint64_t bytes = packed.size() * sizeof(float);
    ljPairsBuffer_ = WGPUContext::instance().createBuffer(bytes,
                                                         wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst,
                                                         "GpuPairForce_LJPairs");
    WGPUContext::instance().queue()->writeBuffer(*ljPairsBuffer_, 0, packed.data(), bytes);
    ljTableUploaded_ = true;
}

void GpuPairForceCompute::ensureBufferCapacity(size_t mobileCount, size_t totalCount, size_t neighborCount) {
    // Буферы атомов и соседей входят в bind group, который разыменовывает их
    // указатели. При totalCount==0 / neighborCount==0 условия роста "> capacity"
    // не срабатывают (0 > 0 ложно), буферы остались бы null и bind group упал бы.
    // Минимум 1, как в resident-пути GpuResidentPhysics::ensureCapacity.
    totalCount = std::max<size_t>(totalCount, 1);
    neighborCount = std::max<size_t>(neighborCount, 1);

    const wgpu::BufferUsage storageUsage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    const wgpu::BufferUsage forceUsage = storageUsage | wgpu::BufferUsage::CopySrc;

    bool atomGrew = false;
    if (totalCount > atomCapacity_) {
        const size_t newCapacity = kAtomCapacityHeadroom(totalCount);
        positionsBuffer_ = WGPUContext::instance().createBuffer(newCapacity * sizeof(float) * 4, storageUsage,
                                                                "GpuPairForce_Positions");
        typesBuffer_ = WGPUContext::instance().createBuffer(newCapacity * sizeof(uint32_t), storageUsage,
                                                            "GpuPairForce_Types");
        forcesBuffer_ = WGPUContext::instance().createBuffer(newCapacity * sizeof(float) * 4, forceUsage,
                                                             "GpuPairForce_Forces");
        readbackBuffer_ = WGPUContext::instance().createBuffer(newCapacity * sizeof(float) * 4,
                                                                wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                                                "GpuPairForce_Readback");
        atomCapacity_ = newCapacity;
        atomGrew = true;
    }

    bool nlOffsetsGrew = false;
    if (mobileCount + 1 > nlOffsetsCapacity_) {
        const size_t newCapacity = kAtomCapacityHeadroom(mobileCount + 1);
        nlOffsetsBuffer_ = WGPUContext::instance().createBuffer(newCapacity * sizeof(uint32_t), storageUsage,
                                                                "GpuPairForce_NLOffsets");
        nlOffsetsCapacity_ = newCapacity;
        nlOffsetsGrew = true;
    }

    bool nlNeighborsGrew = false;
    if (neighborCount > nlNeighborsCapacity_) {
        const size_t newCapacity = kAtomCapacityHeadroom(neighborCount);
        nlNeighborsBuffer_ = WGPUContext::instance().createBuffer(newCapacity * sizeof(uint32_t), storageUsage,
                                                                  "GpuPairForce_NLNeighbors");
        nlNeighborsCapacity_ = newCapacity;
        nlNeighborsGrew = true;
    }

    // Перерос → буфер другой → надо перезалить content + bind group + reset
    // соответствующие upload-flags.
    if (atomGrew) {
        typesUploaded_ = false;
    }
    if (nlOffsetsGrew || nlNeighborsGrew) {
        nlUploaded_ = false;
    }

    if (atomGrew || nlOffsetsGrew || nlNeighborsGrew || !bindGroupValid_) {
        std::array<wgpu::BindGroupEntry, 7> bgEntries{};
        bgEntries[0].binding = 0;
        bgEntries[0].buffer = *uniformBuffer_;
        bgEntries[0].size = sizeof(ComputeUniforms);
        bgEntries[1].binding = 1;
        bgEntries[1].buffer = *positionsBuffer_;
        bgEntries[1].size = atomCapacity_ * sizeof(float) * 4;
        bgEntries[2].binding = 2;
        bgEntries[2].buffer = *typesBuffer_;
        bgEntries[2].size = atomCapacity_ * sizeof(uint32_t);
        bgEntries[3].binding = 3;
        bgEntries[3].buffer = *nlOffsetsBuffer_;
        bgEntries[3].size = nlOffsetsCapacity_ * sizeof(uint32_t);
        bgEntries[4].binding = 4;
        bgEntries[4].buffer = *nlNeighborsBuffer_;
        bgEntries[4].size = nlNeighborsCapacity_ * sizeof(uint32_t);
        bgEntries[5].binding = 5;
        bgEntries[5].buffer = *ljPairsBuffer_;
        bgEntries[5].size = static_cast<uint64_t>(AtomData::Type::COUNT) * static_cast<uint64_t>(AtomData::Type::COUNT) * sizeof(float) * 2;
        bgEntries[6].binding = 6;
        bgEntries[6].buffer = *forcesBuffer_;
        bgEntries[6].size = atomCapacity_ * sizeof(float) * 4;

        bindGroup_ = WGPUContext::instance().createBindGroup(*bindGroupLayout_, bgEntries, "GpuPairForce_BG");
        bindGroupValid_ = true;
    }
}

void GpuPairForceCompute::uploadInputs(const AtomStorage& atoms, const NeighborList& neighborList) {
    const size_t n = atoms.size();

    // Positions меняются каждый physics step — заливаем всегда.
    std::vector<float> posData(n * 4);
    for (size_t i = 0; i < n; ++i) {
        posData[i * 4 + 0] = atoms.posX(i);
        posData[i * 4 + 1] = atoms.posY(i);
        posData[i * 4 + 2] = atoms.posZ(i);
        posData[i * 4 + 3] = 0.0f;
    }
    WGPUContext::instance().queue()->writeBuffer(*positionsBuffer_, 0, posData.data(), n * sizeof(float) * 4);

    // Types не меняются между addAtom/removeAtom — кэшируем на GPU.
    if (!typesUploaded_) {
        std::vector<uint32_t> typeData(n);
        for (size_t i = 0; i < n; ++i) {
            typeData[i] = static_cast<uint32_t>(atoms.type(i));
        }
        WGPUContext::instance().queue()->writeBuffer(*typesBuffer_, 0, typeData.data(), n * sizeof(uint32_t));
        typesUploaded_ = true;
    }

    // NL живёт на GPU между rebuilds. invalidateNeighborList() должен дёргаться
    // после очередного NeighborList::build на CPU стороне.
    if (!nlUploaded_) {
        const auto& offsets = neighborList.offsets();
        const auto& neighbors = neighborList.neighbors();
        WGPUContext::instance().queue()->writeBuffer(*nlOffsetsBuffer_, 0, offsets.data(),
                                                     offsets.size() * sizeof(uint32_t));
        if (!neighbors.empty()) {
            WGPUContext::instance().queue()->writeBuffer(*nlNeighborsBuffer_, 0, neighbors.data(),
                                                         neighbors.size() * sizeof(uint32_t));
        }
        nlUploaded_ = true;
    }

    // Forces начинаем с тех значений, что уже в atoms (там сидят wall-силы).
    std::vector<float> forceData(n * 4);
    for (size_t i = 0; i < n; ++i) {
        forceData[i * 4 + 0] = atoms.forceX(i);
        forceData[i * 4 + 1] = atoms.forceY(i);
        forceData[i * 4 + 2] = atoms.forceZ(i);
        forceData[i * 4 + 3] = atoms.energy(i);
    }
    WGPUContext::instance().queue()->writeBuffer(*forcesBuffer_, 0, forceData.data(), n * sizeof(float) * 4);
}

void GpuPairForceCompute::dispatch(uint32_t mobileCount) {
    wgpu::Device device = *WGPUContext::instance().device();
    wgpu::CommandEncoder encoder = device.createCommandEncoder({});

    wgpu::ComputePassDescriptor passDesc{};
    wgpu::ComputePassEncoder pass = encoder.beginComputePass(passDesc);
    pass.setPipeline(*pipeline_);
    pass.setBindGroup(0, *bindGroup_, 0, nullptr);
    const uint32_t workgroups = (mobileCount + 63u) / 64u;
    pass.dispatchWorkgroups(workgroups, 1, 1);
    pass.end();

    // Копия forces → readback (CopyDst → MapRead).
    encoder.copyBufferToBuffer(*forcesBuffer_, 0, *readbackBuffer_, 0, mobileCount * sizeof(float) * 4);

    wgpu::CommandBuffer cmd = encoder.finish({});
    WGPUContext::instance().queue()->submit(1, &cmd);
}

namespace {
struct MapContext {
    bool done = false;
    bool ok = false;
};
} // namespace

void GpuPairForceCompute::readbackForces(AtomStorage& atoms) {
    const size_t mobileCount = atoms.mobileCount();
    const size_t bytes = mobileCount * sizeof(float) * 4;

    MapContext ctx{};

    wgpu::BufferMapCallbackInfo callbackInfo{};
    callbackInfo.mode = wgpu::CallbackMode::AllowSpontaneous;
    callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
        auto* c = static_cast<MapContext*>(userdata1);
        c->done = true;
        c->ok = (status == WGPUMapAsyncStatus_Success);
    };
    callbackInfo.userdata1 = &ctx;
    callbackInfo.userdata2 = nullptr;

    readbackBuffer_->mapAsync(wgpu::MapMode::Read, 0, bytes, callbackInfo);

    wgpu::Device device = *WGPUContext::instance().device();
    while (!ctx.done) {
        device.poll(true, nullptr);
    }
    if (!ctx.ok) {
        throw std::runtime_error("GpuPairForceCompute: readback map failed");
    }

    const float* data = static_cast<const float*>(readbackBuffer_->getConstMappedRange(0, bytes));
    for (size_t i = 0; i < mobileCount; ++i) {
        atoms.forceX(i) = data[i * 4 + 0];
        atoms.forceY(i) = data[i * 4 + 1];
        atoms.forceZ(i) = data[i * 4 + 2];
        atoms.energy(i) = data[i * 4 + 3];
    }
    readbackBuffer_->unmap();
}

// Нерезидентный GPU-путь LJ: каждый вызов заливает позиции на GPU и читает
// силы обратно (CPU round-trip на шаг). Это первый GPU-бэкенд и эталон
// корректности (~7.5x над CPU, см. BM_GpuPairForce); в боевом GPU-режиме его
// сменяет GpuResidentPhysics, держащий позиции/силы/NL в VRAM без round-trip.
void GpuPairForceCompute::compute(AtomStorage& atoms, const NeighborList& neighborList, const LJForceField& ljForceField) {
    ensureInitialized();
    uploadLJPairTable(ljForceField);

    const size_t mobileCount = atoms.mobileCount();
    const size_t totalCount = atoms.size();
    const size_t neighborCount = neighborList.neighbors().size();

    ensureBufferCapacity(mobileCount, totalCount, neighborCount);
    uploadInputs(atoms, neighborList);

    ComputeUniforms uniforms{};
    uniforms.cutoffSqr = neighborList.cutoff() * neighborList.cutoff();
    uniforms.epsilon = Consts::Epsilon;
    uniforms.mobileCount = static_cast<uint32_t>(mobileCount);
    uniforms.typeCount = static_cast<uint32_t>(AtomData::Type::COUNT);
    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer_, 0, &uniforms, sizeof(uniforms));

    dispatch(static_cast<uint32_t>(mobileCount));
    readbackForces(atoms);
}
