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

    const uint64_t vec4Bytes = count * sizeof(AtomVec4);
    const uint64_t f32Bytes = count * sizeof(float);
    const wgpu::BufferUsage usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;

    atomLayer_.sbPos = WGPUContext::instance().createBuffer(vec4Bytes, usage, "Atoms_Pos");
    atomLayer_.sbVel = WGPUContext::instance().createBuffer(vec4Bytes, usage, "Atoms_Vel");
    atomLayer_.sbType = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Type");
    atomLayer_.sbRadius = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Radius");
    atomLayer_.sbSel = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Selection");
    atomLayer_.storageCapacity = count;

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

void RendererWGPU::drawAtomsImpl(const RenderAtomsView& atoms, const RenderData& renderData, bool applySelection) {
    const size_t count = atoms.count;
    if (count == 0 || !atoms.hasPositions() || !atoms.hasTypes()) {
        return;
    }

    ensureStorageBuffers(count);

    atomLayer_.posData.resize(count);
    atomLayer_.velData.resize(count);
    atomLayer_.radii.resize(count);
    atomLayer_.typeData.resize(count);
    atomLayer_.selectedData.assign(count, 0.0f);

    for (size_t i = 0; i < count; ++i) {
        atomLayer_.posData[i] = {atoms.x[i], atoms.y[i], atoms.z[i]};
        atomLayer_.velData[i] = atoms.hasVelocities() ? AtomVec4{atoms.vx[i], atoms.vy[i], atoms.vz[i]} : AtomVec4{};
        const AtomData::Type atomType = static_cast<AtomData::Type>(atoms.type[i]);
        atomLayer_.radii[i] = atoms.hasRadii() ? atoms.radius[i] : AtomData::getProps(atomType).radius;
        atomLayer_.typeData[i] = static_cast<float>(atomType);
    }
    if (applySelection) {
        for (const size_t idx : renderData.selectedAtomIndices) {
            if (idx < count) {
                atomLayer_.selectedData[idx] = 1.0f;
            }
        }
    }

    uploadStorageBuffer(*atomLayer_.sbPos, atomLayer_.posData.data(), count);
    uploadStorageBuffer(*atomLayer_.sbVel, atomLayer_.velData.data(), count);
    uploadStorageBuffer(*atomLayer_.sbRadius, atomLayer_.radii.data(), count);
    uploadStorageBuffer(*atomLayer_.sbType, atomLayer_.typeData.data(), count);
    uploadStorageBuffer(*atomLayer_.sbSel, atomLayer_.selectedData.data(), count);

    float maxSpeedSqr = 1.f;
    if (renderData.speedColorMode != RenderData::SpeedColorMode::AtomColor) {
        if (renderData.speedGradientMax > 0.f) {
            maxSpeedSqr = renderData.speedGradientMax * renderData.speedGradientMax;
        }
        else if (atoms.hasVelocities()) {
            const auto it = std::ranges::max_element(std::views::iota(size_t{0}, count), {}, [&](size_t i) {
                return atoms.vx[i] * atoms.vx[i] + atoms.vy[i] * atoms.vy[i] + atoms.vz[i] * atoms.vz[i];
            });
            const float speedSqr = atoms.vx[*it] * atoms.vx[*it] + atoms.vy[*it] * atoms.vy[*it] + atoms.vz[*it] * atoms.vz[*it];
            maxSpeedSqr = std::max(1e-6f, speedSqr);
        }
    }
    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, offsetof(SceneUniforms, maxSpeedSqr), &maxSpeedSqr, sizeof(float));

    currentPass->setPipeline(*pipelines_.atomSphere);
    currentPass->setBindGroup(0, *atomLayer_.bindGroup, 0, nullptr);
    currentPass->setVertexBuffer(0, *atomLayer_.atomQuadVb, 0, atomLayer_.atomQuadVb->getSize());
    currentPass->draw(6, count, 0, 0);
}
