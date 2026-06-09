#include "Render.h"

#include <algorithm>
#include <ranges>

#include "Lattice/Engine/physics/Atom/AtomData.h"
#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::ensureStorageBuffers(size_t count) {
    if (count <= sbCapacity_) {
        return;
    }

    const uint64_t vec4Bytes = count * sizeof(AtomVec4);
    const uint64_t f32Bytes = count * sizeof(float);
    const wgpu::BufferUsage usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;

    sbPos = WGPUContext::instance().createBuffer(vec4Bytes, usage, "Atoms_Pos");
    sbVel = WGPUContext::instance().createBuffer(vec4Bytes, usage, "Atoms_Vel");
    sbType = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Type");
    sbRadius = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Radius");
    sbSel = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Selection");
    sbCapacity_ = count;

    std::array<wgpu::BindGroupEntry, 6> entries{};
    entries[0].binding = 0;
    entries[0].buffer = *uniformBuffer;
    entries[0].size = sizeof(SceneUniforms);
    entries[1].binding = 1;
    entries[1].buffer = *sbPos;
    entries[1].size = vec4Bytes;
    entries[2].binding = 2;
    entries[2].buffer = *sbVel;
    entries[2].size = vec4Bytes;
    entries[3].binding = 3;
    entries[3].buffer = *sbType;
    entries[3].size = f32Bytes;
    entries[4].binding = 4;
    entries[4].buffer = *sbRadius;
    entries[4].size = f32Bytes;
    entries[5].binding = 5;
    entries[5].buffer = *sbSel;
    entries[5].size = f32Bytes;

    atomBindGroup = WGPUContext::instance().createBindGroup(*atomBindGroupLayout, entries, "AtomBindGroup");
}

template <typename T> void RendererWGPU::uploadStorageBuffer(wgpu::Buffer& buf, const T* data, size_t count) {
    WGPUContext::instance().queue()->writeBuffer(buf, 0, data, count * sizeof(T));
}

wgpu::BindGroup RendererWGPU::ensureAtomBindGroup(size_t boundCount, const RenderAtomsGpuResidency& residency) {
    // CPU-режим: обычная renderer-owned bind-group (собрана в ensureStorageBuffers).
    // Путь не меняется ни на байт относительно прежнего поведения.
    if (!residency.valid) {
        return *atomBindGroup;
    }

    // GPU-режим (zero-copy): bind-group биндит РЕЗИДЕНТНЫЕ pos/vel (binding 1/2) +
    // renderer-owned type/radius/sel (binding 3/4/5). Layout тот же. Резидентный
    // буфер — ЧУЖОЙ (принадлежит GpuResidentPhysics через App-слой) и может переехать
    // при росте сцены, поэтому пере-собираем по кеш-ключу:
    //   (pos-буфер как идентичность источника, residency.generation, наша sbCapacity_, boundCount).
    // boundCount в ключе: BindGroupEntry.size резидентных = boundCount*16 (а не полный
    // буфер), а boundCount может меняться на transient-правке сцены при той же generation
    // — тогда пере-биндим, чтобы bound size был ровно текущим. На статичной сцене ключ
    // неизменен → горячий путь bind-group не трогает.
    const WGPUBuffer posRaw = residency.positions;
    const bool keyMatch = gpuBindValid_ && gpuBindPosBuffer_ == posRaw && gpuBindGeneration_ == residency.generation &&
                          gpuBindSbCapacity_ == sbCapacity_ && gpuBindBoundCount_ == boundCount;
    if (keyMatch) {
        return *atomBindGroupGpu_;
    }

    const uint64_t residentBytes = static_cast<uint64_t>(boundCount) * sizeof(AtomVec4); // boundCount*16
    const uint64_t f32Bytes = static_cast<uint64_t>(sbCapacity_) * sizeof(float);

    std::array<wgpu::BindGroupEntry, 6> entries{};
    entries[0].binding = 0;
    entries[0].buffer = *uniformBuffer;
    entries[0].size = sizeof(SceneUniforms);
    entries[1].binding = 1; // резидентные позиции физики (zero-copy)
    entries[1].buffer = residency.positions;
    entries[1].size = residentBytes;
    entries[2].binding = 2; // резидентные скорости физики (zero-copy, нужны speed-color)
    entries[2].buffer = residency.velocities;
    entries[2].size = residentBytes;
    entries[3].binding = 3; // type/radius/sel остаются renderer-owned
    entries[3].buffer = *sbType;
    entries[3].size = f32Bytes;
    entries[4].binding = 4;
    entries[4].buffer = *sbRadius;
    entries[4].size = f32Bytes;
    entries[5].binding = 5;
    entries[5].buffer = *sbSel;
    entries[5].size = f32Bytes;

    atomBindGroupGpu_ = WGPUContext::instance().createBindGroup(*atomBindGroupLayout, entries, "AtomBindGroupGpu");

    gpuBindValid_ = true;
    gpuBindPosBuffer_ = posRaw;
    gpuBindGeneration_ = residency.generation;
    gpuBindSbCapacity_ = sbCapacity_;
    gpuBindBoundCount_ = boundCount;
    return *atomBindGroupGpu_;
}

void RendererWGPU::drawAtomsImpl(const RenderAtomsView& atoms, const RenderData& renderData, bool applySelection) {
    const size_t count = atoms.count;
    // В чистом zero-copy GPU-режиме CPU-указатели pos могут быть НЕ свежими (download
    // пропущен App-слоем). Позиции/типы для отрисовки тогда живут в VRAM. Поэтому в
    // GPU-режиме допустимо count>0 даже если !hasPositions(): рисуем из резидентного
    // буфера. Требуем CPU-позиции ТОЛЬКО в CPU-режиме (residency.valid==false).
    const RenderAtomsGpuResidency& residency = atoms.gpu;
    const bool gpuMode = residency.valid;
    if (count == 0 || !atoms.hasTypes() || (!gpuMode && !atoms.hasPositions())) {
        return;
    }

    // В GPU-режиме рисуем ровно столько атомов, сколько залито в VRAM — clamp на
    // transient-правке сцены: CPU AtomStorage уже изменён, а резидентный re-upload
    // отложен (напр. на паузе), поэтому boundCount = min(count, residency.boundCount)
    // НИКОГДА не читает за пределами меньшего из буферов. В CPU-режиме boundCount == count.
    const size_t boundCount = gpuMode ? std::min<size_t>(count, residency.boundCount) : count;
    if (boundCount == 0) {
        return; // VRAM ещё пуст (например, рост сцены до первого upload) — нечего рисовать
    }

    ensureStorageBuffers(count);

    // type/radius/selection остаются renderer-owned В ОБОИХ режимах (формат type в
    // физике u32, radius физике неизвестен, selection — render/UI-состояние). Пакуем
    // и заливаем их всегда. pos/vel — только в CPU-режиме (в GPU их читаем zero-copy).
    radii.resize(count);
    typeData.resize(count);
    selectedData.assign(count, 0.0f);

    if (!gpuMode) {
        posData_.resize(count);
        velData_.resize(count);
    }

    for (size_t i = 0; i < count; ++i) {
        if (!gpuMode) {
            posData_[i] = {atoms.x[i], atoms.y[i], atoms.z[i]};
            velData_[i] = atoms.hasVelocities() ? AtomVec4{atoms.vx[i], atoms.vy[i], atoms.vz[i]} : AtomVec4{};
        }
        const AtomData::Type atomType = static_cast<AtomData::Type>(atoms.type[i]);
        radii[i] = atoms.hasRadii() ? atoms.radius[i] : AtomData::getProps(atomType).radius;
        typeData[i] = static_cast<float>(atomType);
    }
    if (applySelection) {
        for (const size_t idx : renderData.selectedAtomIndices) {
            if (idx < count) {
                selectedData[idx] = 1.0f;
            }
        }
    }

    // pos/vel pack+upload — это и есть убранная per-draw работа в GPU-режиме.
    if (!gpuMode) {
        uploadStorageBuffer(*sbPos, posData_.data(), count);
        uploadStorageBuffer(*sbVel, velData_.data(), count);
    }
    uploadStorageBuffer(*sbRadius, radii.data(), count);
    uploadStorageBuffer(*sbType, typeData.data(), count);
    uploadStorageBuffer(*sbSel, selectedData.data(), count);

    float maxSpeedSqr = 1.f;
    if (renderData.speedColorMode != RenderData::SpeedColorMode::AtomColor) {
        if (renderData.speedGradientMax > 0.f) {
            maxSpeedSqr = renderData.speedGradientMax * renderData.speedGradientMax;
        }
        else if (atoms.hasVelocities()) {
            // Auto-max сканирует CPU-скорости. В GPU-режиме их свежесть гарантирует
            // условный per-frame sync App-слоя при активной speed-color-auto ветке;
            // если hasVelocities() ложно — остаёмся на 1.0f.
            const auto it = std::ranges::max_element(std::views::iota(size_t{0}, count), {}, [&](size_t i) {
                return atoms.vx[i] * atoms.vx[i] + atoms.vy[i] * atoms.vy[i] + atoms.vz[i] * atoms.vz[i];
            });
            const float speedSqr = atoms.vx[*it] * atoms.vx[*it] + atoms.vy[*it] * atoms.vy[*it] + atoms.vz[*it] * atoms.vz[*it];
            maxSpeedSqr = std::max(1e-6f, speedSqr);
        }
    }
    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, offsetof(SceneUniforms, maxSpeedSqr), &maxSpeedSqr, sizeof(float));

    currentPass->setPipeline(*atomPipeline);
    currentPass->setBindGroup(0, ensureAtomBindGroup(boundCount, residency), 0, nullptr);
    currentPass->setVertexBuffer(0, *atomQuadVb, 0, atomQuadVb->getSize());
    currentPass->draw(6, static_cast<uint32_t>(boundCount), 0, 0);
}
