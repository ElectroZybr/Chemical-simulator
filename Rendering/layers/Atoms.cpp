#include "Rendering/Renderer.h"

#include <algorithm>
#include <cstddef>
#include <ranges>

#include "Lattice/Engine/physics/Atom/AtomData.h"
#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::ensureStorageBuffers(size_t count) {
    if (count <= atomLayer_.storageCapacity) {
        return;
    }

    const size_t newCapacity = atomLayer_.storageCapacity == 0 ? count : std::max(count, atomLayer_.storageCapacity * 2);
    const uint64_t vec4Bytes = static_cast<uint64_t>(newCapacity) * sizeof(AtomVec4);
    const uint64_t f32Bytes = static_cast<uint64_t>(newCapacity) * sizeof(float);
    const wgpu::BufferUsage usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;

    atomLayer_.sbPos = WGPUContext::instance().createBuffer(vec4Bytes, usage, "Atoms_Pos");
    atomLayer_.sbVel = WGPUContext::instance().createBuffer(vec4Bytes, usage, "Atoms_Vel");
    atomLayer_.sbType = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Type");
    atomLayer_.sbRadius = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Radius");
    atomLayer_.sbSel = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Selection");
    atomLayer_.storageCapacity = newCapacity;

    std::array<wgpu::BindGroupEntry, 6> entries{};
    entries[0].binding = 0;
    entries[0].buffer = *uniformBuffer;
    entries[0].size = sizeof(SceneUniforms);
    entries[1].binding = 1;
    entries[1].buffer = *atomLayer_.sbPos;
    entries[1].size = vec4Bytes;
    entries[2].binding = 2;
    entries[2].buffer = *atomLayer_.sbVel;
    entries[2].size = vec4Bytes;
    entries[3].binding = 3;
    entries[3].buffer = *atomLayer_.sbType;
    entries[3].size = f32Bytes;
    entries[4].binding = 4;
    entries[4].buffer = *atomLayer_.sbRadius;
    entries[4].size = f32Bytes;
    entries[5].binding = 5;
    entries[5].buffer = *atomLayer_.sbSel;
    entries[5].size = f32Bytes;

    atomLayer_.bindGroup = WGPUContext::instance().createBindGroup(*atomLayer_.bindGroupLayout, entries, "AtomBindGroup");
}

template <typename T> void RendererWGPU::uploadStorageBuffer(wgpu::Buffer& buf, const T* data, size_t count) {
    WGPUContext::instance().queue()->writeBuffer(buf, 0, data, count * sizeof(T));
}

template void RendererWGPU::uploadStorageBuffer<AtomVec4>(wgpu::Buffer& buf, const AtomVec4* data, size_t count);
template void RendererWGPU::uploadStorageBuffer<float>(wgpu::Buffer& buf, const float* data, size_t count);

void RendererWGPU::initAtomColors() {
    atomLayer_.typeColorsData.assign(119, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    for (size_t i = 0; i < static_cast<size_t>(AtomData::Type::COUNT) && i < atomLayer_.typeColorsData.size(); ++i) {
        const auto& props = AtomData::getProps(static_cast<AtomData::Type>(i));
        atomLayer_.typeColorsData[i] = glm::vec4(props.color.r / 255.0f, props.color.g / 255.0f, props.color.b / 255.0f, props.color.a / 255.0f);
    }
}

void RendererWGPU::initAtomQuadBuffer() {
    static constexpr float quad[] = {
        -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
    };
    WGPUContext& ctx = WGPUContext::instance();
    atomLayer_.atomQuadVb = ctx.createBuffer(sizeof(quad), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Atom_Quad_Geometry");
    ctx.queue()->writeBuffer(*atomLayer_.atomQuadVb, 0, quad, sizeof(quad));
}

bool RendererWGPU::prepareAtomsCpuData(const View::RenderAtomsView& atoms, const RenderData& renderData, bool applySelection) {
    const size_t count = atoms.count;
    if (count == 0 || !atoms.hasPositions() || !atoms.hasTypes()) {
        return false;
    }

    const bool hasVelocities = atoms.hasVelocities();
    const bool hasRadii = atoms.hasRadii();

    atomLayer_.posData.resize(count);
    atomLayer_.velData.resize(count);
    atomLayer_.radii.resize(count);
    atomLayer_.typeData.resize(count);
    atomLayer_.selectedData.assign(count, 0.0f);

    const float* x = atoms.x;
    const float* y = atoms.y;
    const float* z = atoms.z;
    const float* vx = atoms.vx;
    const float* vy = atoms.vy;
    const float* vz = atoms.vz;
    const uint8_t* type = atoms.type;
    const float* radius = atoms.radius;

    for (size_t i = 0; i < count; ++i) {
        atomLayer_.posData[i] = {x[i], y[i], z[i]};

        if (hasVelocities) {
            atomLayer_.velData[i] = AtomVec4{vx[i], vy[i], vz[i]};
        } else {
            atomLayer_.velData[i] = AtomVec4{};
        }

        const AtomData::Type atomType = static_cast<AtomData::Type>(type[i]);
        atomLayer_.typeData[i] = static_cast<float>(atomType);
        atomLayer_.radii[i] = hasRadii ? radius[i] : AtomData::getProps(atomType).radius;
    }

    if (applySelection) {
        for (const size_t idx : renderData.selectedAtomIndices) {
            if (idx < count) {
                atomLayer_.selectedData[idx] = 1.0f;
            }
        }
    }

    return true;
}

void RendererWGPU::uploadPreparedAtomsGpu(const View::RenderAtomsView& atoms, const RenderData& renderData) {
    const size_t count = atoms.count;
    if (count == 0 || atomLayer_.posData.size() != count || atomLayer_.typeData.size() != count) {
        return;
    }

    ensureStorageBuffers(count);

    uploadStorageBuffer(*atomLayer_.sbPos, atomLayer_.posData.data(), count);
    uploadStorageBuffer(*atomLayer_.sbVel, atomLayer_.velData.data(), count);
    uploadStorageBuffer(*atomLayer_.sbRadius, atomLayer_.radii.data(), count);
    uploadStorageBuffer(*atomLayer_.sbType, atomLayer_.typeData.data(), count);
    uploadStorageBuffer(*atomLayer_.sbSel, atomLayer_.selectedData.data(), count);

    float maxSpeedSqr = 1.f;
    if (renderData.speedColorMode != RenderData::SpeedColorMode::AtomColor) {
        if (renderData.speedGradientMax > 0.f) {
            maxSpeedSqr = renderData.speedGradientMax * renderData.speedGradientMax;
        } else if (atoms.hasVelocities()) {
            maxSpeedSqr = 1e-6f;
            const float* vx = atoms.vx;
            const float* vy = atoms.vy;
            const float* vz = atoms.vz;
            for (size_t i = 0; i < count; ++i) {
                const float speedSqr = vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i];
                maxSpeedSqr = std::max(maxSpeedSqr, speedSqr);
            }
        }
    }

    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, offsetof(SceneUniforms, maxSpeedSqr), &maxSpeedSqr, sizeof(float));
}

void RendererWGPU::drawPreparedAtomsGpu(size_t count) {
    if (count == 0 || !atomLayer_.bindGroup || !atomLayer_.atomQuadVb || !currentPass) {
        return;
    }

    currentPass->setPipeline(*pipelines_.atomSphere);
    currentPass->setBindGroup(0, *atomLayer_.bindGroup, 0, nullptr);
    currentPass->setVertexBuffer(0, *atomLayer_.atomQuadVb, 0, atomLayer_.atomQuadVb->getSize());
    currentPass->draw(6, count, 0, 0);
}

void RendererWGPU::drawAtomsImpl(const View::RenderAtomsView& atoms, const RenderData& renderData, bool applySelection) {
    if (!prepareAtomsCpuData(atoms, renderData, applySelection)) {
        return;
    }

    uploadPreparedAtomsGpu(atoms, renderData);
    drawPreparedAtomsGpu(atoms.count);
}
