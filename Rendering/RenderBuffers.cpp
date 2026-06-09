#include "Render.h"

#include "Lattice/Engine/physics/Atom/AtomData.h"
#include "Rendering/backend/WGPUContext.h"

RendererWGPU::RendererWGPU() : surfaceFormat(WGPUContext::instance().surfaceFormat()) {
    uniformBuffer = WGPUContext::instance().createBuffer(sizeof(SceneUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "RenderingUniforms");

    initAtomColors();
    initAtomQuadBuffer();
    initBoxBuffer();
    initBondBuffer();
    initGridLineBuffer();
    initMemoryOrderBuffer();
}

void RendererWGPU::initAtomColors() {
    typeColorsData.assign(119, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    for (size_t i = 0; i < static_cast<size_t>(AtomData::Type::COUNT) && i < typeColorsData.size(); ++i) {
        const auto& props = AtomData::getProps(static_cast<AtomData::Type>(i));
        typeColorsData[i] = glm::vec4(props.color.r / 255.0f, props.color.g / 255.0f, props.color.b / 255.0f, props.color.a / 255.0f);
    }
}

void RendererWGPU::initAtomQuadBuffer() {
    static constexpr float quad[] = {
        -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
    };
    WGPUContext& ctx = WGPUContext::instance();
    atomQuadVb = ctx.createBuffer(sizeof(quad), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Atom_Quad_Geometry");
    ctx.queue()->writeBuffer(*atomQuadVb, 0, quad, sizeof(quad));
}

void RendererWGPU::initBoxBuffer() {
    boxVb = WGPUContext::instance().createBuffer(24 * 3 * sizeof(float), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Box_Geometry");
}

void RendererWGPU::initBondBuffer() {
    bondVb = WGPUContext::instance().createBuffer(128, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Bond_Geometry");
    bondVbCapacity_ = 128;
}

void RendererWGPU::initGridLineBuffer() {
    const float lines[] = {
        0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
        1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1,
    };
    WGPUContext& ctx = WGPUContext::instance();
    gridLineVb = ctx.createBuffer(sizeof(lines), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Grid_Cell_Unit_Lines");
    ctx.queue()->writeBuffer(*gridLineVb, 0, lines, sizeof(lines));
}

void RendererWGPU::initMemoryOrderBuffer() {
    memoryOrderVb = WGPUContext::instance().createBuffer(128, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Memory_Order_Geometry");
    memoryOrderVbCapacity_ = 128;
}
