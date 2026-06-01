#include "RendererWGPU.h"

#include <algorithm>
#include <cstring>
#include <ranges>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Lattice/Engine/physics/AtomData.h"
#include "Rendering/WGPUContext.h"

#ifdef True
#undef True
#endif

#ifdef False
#undef False
#endif

namespace {
    wgpu::ShaderModule createShaderModule(std::string_view wgsl) {
        WGPUShaderSourceWGSL wgslDesc{};
        wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
        wgslDesc.code = wgpu::StringView(wgsl);

        wgpu::ShaderModuleDescriptor desc{};
        desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
        return WGPUContext::instance().device()->createShaderModule(desc);
    }

}

RendererWGPU::RendererWGPU() : surfaceFormat(WGPUContext::instance().surfaceFormat()) {
    uniformBuffer = WGPUContext::instance().createBuffer(sizeof(SceneUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                         "RenderingUniforms");

    initAtomColors();
    initAtomQuadBuffer();
    initBoxBuffer();
    initBondBuffer();
    initGridLineBuffer();
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
    boxVb = WGPUContext::instance().createBuffer(24 * 3 * sizeof(float), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst,
                                                 "Box_Geometry");
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

void RendererWGPU::initAtomPipeline(std::string_view atomWGSL) {
    wgpu::ShaderModule shader = createShaderModule(atomWGSL);

    std::array<wgpu::BindGroupLayoutEntry, 6> entries;
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    entries[0].buffer.type = wgpu::BufferBindingType::Uniform;

    for (int i = 1; i <= 5; ++i) {
        entries[i].binding = i;
        entries[i].visibility = wgpu::ShaderStage::Vertex;
        entries[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    }

    atomBindGroupLayout = WGPUContext::instance().createBindGroupLayout(entries, "AtomBindGroupLayout");

    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView("AtomPipelineLayout");
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&atomBindGroupLayout;
    wgpu::PipelineLayout pipelineLayout = WGPUContext::instance().device()->createPipelineLayout(plDesc);

    wgpu::VertexAttribute quadAttr;
    quadAttr.format = wgpu::VertexFormat::Float32x2;
    quadAttr.offset = 0;
    quadAttr.shaderLocation = 0;

    wgpu::VertexBufferLayout quadLayout;
    quadLayout.arrayStride = 2 * sizeof(float);
    quadLayout.stepMode = wgpu::VertexStepMode::Vertex;
    quadLayout.attributeCount = 1;
    quadLayout.attributes = &quadAttr;

    wgpu::ColorTargetState colorTarget;
    colorTarget.format = surfaceFormat;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::BlendState blend;
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha = blend.color;
    colorTarget.blend = &blend;

    wgpu::FragmentState fragState;
    fragState.module = shader;
    fragState.entryPoint = wgpu::StringView("fs_main");
    fragState.targetCount = 1;
    fragState.targets = &colorTarget;

    wgpu::DepthStencilState depthState;
    depthState.format = wgpu::TextureFormat::Depth24Plus;
    depthState.depthWriteEnabled = wgpu::OptionalBool::True;
    depthState.depthCompare = wgpu::CompareFunction::Less;

    wgpu::RenderPipelineDescriptor pDesc;
    pDesc.label = wgpu::StringView("AtomPipeline");
    pDesc.layout = pipelineLayout;
    pDesc.vertex.module = shader;
    pDesc.vertex.entryPoint = wgpu::StringView("vs_main");
    pDesc.vertex.bufferCount = 1;
    pDesc.vertex.buffers = &quadLayout;
    pDesc.fragment = &fragState;
    pDesc.depthStencil = &depthState;
    pDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pDesc.multisample.count = 1;
    pDesc.multisample.mask = 0xFFFFFFFF;
    pDesc.multisample.alphaToCoverageEnabled = false;

    atomPipeline = WGPUContext::instance().device()->createRenderPipeline(pDesc);
}

void RendererWGPU::initLinePipeline(wgpu::RenderPipeline& outPipeline, std::string_view wgsl) {
    wgpu::ShaderModule shader = createShaderModule(wgsl);

    wgpu::BindGroupLayoutEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.visibility = wgpu::ShaderStage::Vertex;
    uboEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    lineBindGroupLayout = WGPUContext::instance().createBindGroupLayout({&uboEntry, 1}, "LineBindGroupLayout");

    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView("LinePipelineLayout");
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&lineBindGroupLayout;

    wgpu::VertexAttribute attr{};
    attr.format = wgpu::VertexFormat::Float32x3;
    attr.offset = 0;
    attr.shaderLocation = 0;

    wgpu::VertexBufferLayout vbl{};
    vbl.arrayStride = 3 * sizeof(float);
    vbl.stepMode = wgpu::VertexStepMode::Vertex;
    vbl.attributeCount = 1;
    vbl.attributes = &attr;

    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = surfaceFormat;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragState{};
    fragState.module = shader;
    fragState.entryPoint = wgpu::StringView("fs_main");
    fragState.targetCount = 1;
    fragState.targets = &colorTarget;

    wgpu::RenderPipelineDescriptor pDesc{};
    pDesc.label = wgpu::StringView("LineRenderPipeline");
    pDesc.layout = WGPUContext::instance().device()->createPipelineLayout(plDesc);
    pDesc.vertex.module = shader;
    pDesc.vertex.entryPoint = wgpu::StringView("vs_main");
    pDesc.vertex.bufferCount = 1;
    pDesc.vertex.buffers = &vbl;
    pDesc.fragment = &fragState;
    pDesc.primitive.topology = wgpu::PrimitiveTopology::LineList;
    pDesc.multisample.count = 1;
    pDesc.multisample.mask = 0xFFFFFFFF;
    pDesc.multisample.alphaToCoverageEnabled = false;

    wgpu::DepthStencilState depthState{};
    depthState.format = wgpu::TextureFormat::Depth24Plus;
    depthState.depthWriteEnabled = wgpu::OptionalBool::False;
    depthState.depthCompare = wgpu::CompareFunction::Less;
    pDesc.depthStencil = &depthState;

    outPipeline = WGPUContext::instance().device()->createRenderPipeline(pDesc);

    wgpu::BindGroupEntry entry{};
    entry.binding = 0;
    entry.buffer = *uniformBuffer;
    entry.size = sizeof(SceneUniforms);

    lineBindGroup = WGPUContext::instance().createBindGroup(*lineBindGroupLayout, {&entry, 1}, "LineBindGroup");
}

void RendererWGPU::initGridPipeline(std::string_view gridWGSL) {
    wgpu::ShaderModule shader = createShaderModule(gridWGSL);

    wgpu::BindGroupLayoutEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.visibility = wgpu::ShaderStage::Vertex;
    uboEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    gridBindGroupLayout = WGPUContext::instance().createBindGroupLayout({&uboEntry, 1}, "GridBindGroupLayout");

    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView("GridPipelineLayout");
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&gridBindGroupLayout;

    wgpu::VertexAttribute vertAttr{};
    vertAttr.format = wgpu::VertexFormat::Float32x3;
    vertAttr.offset = 0;
    vertAttr.shaderLocation = 0;

    wgpu::VertexBufferLayout vertLayout{};
    vertLayout.arrayStride = 3 * sizeof(float);
    vertLayout.stepMode = wgpu::VertexStepMode::Vertex;
    vertLayout.attributeCount = 1;
    vertLayout.attributes = &vertAttr;

    std::array<wgpu::VertexAttribute, 3> instAttrs{};
    instAttrs[0].format = wgpu::VertexFormat::Float32x4;
    instAttrs[0].offset = offsetof(GridInstance, origin);
    instAttrs[0].shaderLocation = 1;
    instAttrs[1].format = wgpu::VertexFormat::Float32;
    instAttrs[1].offset = offsetof(GridInstance, cellSize);
    instAttrs[1].shaderLocation = 2;
    instAttrs[2].format = wgpu::VertexFormat::Float32;
    instAttrs[2].offset = offsetof(GridInstance, atomCount);
    instAttrs[2].shaderLocation = 3;

    wgpu::VertexBufferLayout instLayout{};
    instLayout.arrayStride = sizeof(GridInstance);
    instLayout.stepMode = wgpu::VertexStepMode::Instance;
    instLayout.attributeCount = instAttrs.size();
    instLayout.attributes = instAttrs.data();

    std::array<wgpu::VertexBufferLayout, 2> vbLayouts = {vertLayout, instLayout};

    wgpu::BlendState blend{};
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha = blend.color;

    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = surfaceFormat;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;
    colorTarget.blend = &blend;

    wgpu::FragmentState fragState{};
    fragState.module = shader;
    fragState.entryPoint = wgpu::StringView("fs_main");
    fragState.targetCount = 1;
    fragState.targets = &colorTarget;

    wgpu::DepthStencilState depthState{};
    depthState.format = wgpu::TextureFormat::Depth24Plus;
    depthState.depthWriteEnabled = wgpu::OptionalBool::False;
    depthState.depthCompare = wgpu::CompareFunction::Less;

    wgpu::RenderPipelineDescriptor pDesc{};
    pDesc.label = wgpu::StringView("GridRenderPipeline");
    pDesc.layout = WGPUContext::instance().device()->createPipelineLayout(plDesc);
    pDesc.vertex.module = shader;
    pDesc.vertex.entryPoint = wgpu::StringView("vs_main");
    pDesc.vertex.bufferCount = vbLayouts.size();
    pDesc.vertex.buffers = vbLayouts.data();
    pDesc.fragment = &fragState;
    pDesc.depthStencil = &depthState;
    pDesc.primitive.topology = wgpu::PrimitiveTopology::LineList;
    pDesc.multisample.count = 1;
    pDesc.multisample.mask = 0xFFFFFFFF;
    pDesc.multisample.alphaToCoverageEnabled = false;

    gridPipeline = WGPUContext::instance().device()->createRenderPipeline(pDesc);

    wgpu::BindGroupEntry entry{};
    entry.binding = 0;
    entry.buffer = *uniformBuffer;
    entry.size = sizeof(SceneUniforms);

    gridBindGroup = WGPUContext::instance().createBindGroup(*gridBindGroupLayout, {&entry, 1}, "GridBindGroup");
}

void RendererWGPU::initBoxPipeline(std::string_view boxWGSL) { initLinePipeline(*boxPipeline, boxWGSL); }
void RendererWGPU::initBondPipeline(std::string_view bondWGSL) { initLinePipeline(*bondPipeline, bondWGSL); }

void RendererWGPU::ensureStorageBuffers(size_t count) {
    if (count <= sbCapacity_) {
        return;
    }

    // Геометрический рост 1.5x с round-up до count: при типичном линейном
    // приросте населения атомов аллокаций становится O(log N), а не O(N), и
    // bind group не пересоздаётся на каждый "+1 атом" из UI.
    size_t newCapacity = sbCapacity_ * 3 / 2 + 1;
    if (newCapacity < count) {
        newCapacity = count;
    }

    const uint64_t vec4Bytes = newCapacity * sizeof(AtomVec4);
    const uint64_t f32Bytes = newCapacity * sizeof(float);
    const wgpu::BufferUsage usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;

    sbPos = WGPUContext::instance().createBuffer(vec4Bytes, usage, "Atoms_Pos");
    sbVel = WGPUContext::instance().createBuffer(vec4Bytes, usage, "Atoms_Vel");
    sbType = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Type");
    sbRadius = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Radius");
    sbSel = WGPUContext::instance().createBuffer(f32Bytes, usage, "Atoms_Selection");
    sbCapacity_ = newCapacity;

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
    // boundCount в ключе: §11/§6.2 требуют BindGroupEntry.size резидентных = boundCount*16
    // (а не полный буфер), а boundCount может меняться на transient-правке сцены при той
    // же generation — тогда пере-биндим, чтобы bound size был ровно текущим. На статичной
    // сцене ключ неизменен → горячий путь bind-group не трогает.
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

void RendererWGPU::drawShot(wgpu::TextureView targetView, wgpu::TextureView depthView) {
    if (getRenderDataCount() == 0) {
        beginPass(targetView, depthView, wgpu::LoadOp::Clear);
        return;
    }

    for (size_t renderDataIndex = 0; renderDataIndex < getRenderDataCount(); ++renderDataIndex) {
        const RenderData& renderData = getRenderData(renderDataIndex);
        drawWorldPass(targetView, depthView, renderData, renderDataIndex == 0 ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
                      renderData.isActiveWorld);
        if (renderDataIndex + 1 < getRenderDataCount()) {
            endFrame();
        }
    }
}

void RendererWGPU::drawWorldPass(wgpu::TextureView targetView, wgpu::TextureView depthView, const RenderData& renderData, wgpu::LoadOp targetLoadOp,
                                 bool applySelection) {
    updateMatrices();

    SceneUniforms uniforms{};
    uniforms.view = view;
    uniforms.projection = projection;
    uniforms.lightDir = glm::vec4(getLightDir(), 0.f);
    uniforms.colorMode = glm::vec4(static_cast<float>(renderData.speedColorMode), 0, 0, 0);
    uniforms.maxSpeedSqr = glm::vec4(1.f, 0, 0, 0);
    uniforms.maxCount = glm::vec4(1.f, 0, 0, 0);
    uniforms.renderOffset = glm::vec4(renderData.renderOffset.x, renderData.renderOffset.y, renderData.renderOffset.z, 0.0f);
    uniforms.lineColor = glm::vec4(0.4f, 0.6f, 1.0f, 0.3f);
    for (size_t i = 0; i < typeColorsData.size(); ++i) {
        uniforms.typeColors[i] = typeColorsData[i];
    }

    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, 0, &uniforms, sizeof(uniforms));

    beginPass(targetView, depthView, targetLoadOp);

    if (renderData.drawBonds && !renderData.bonds.empty()) {
        setLineColor(glm::vec4(0.4f, 0.6f, 1.0f, 0.3f));
        drawBondsImpl(renderData.atoms, renderData.bonds);
    }
    if (renderData.drawGrid && !renderData.grid.empty()) {
        drawGridImpl(renderData.grid);
    }
    if (renderData.drawBox && renderData.hasBox) {
        setLineColor(applySelection ? glm::vec4(0.5f, 0.78f, 1.0f, 0.55f) : glm::vec4(0.35f, 0.52f, 0.9f, 0.3f));
        drawBoxImpl(renderData.worldSize);
    }
    drawAtomsImpl(renderData.atoms, renderData, applySelection);
}

void RendererWGPU::beginPass(wgpu::TextureView targetView, wgpu::TextureView depthView, wgpu::LoadOp targetLoadOp) {
    WGPUContext& ctx = WGPUContext::instance();
    wgpu::CommandEncoderDescriptor encDesc;
    encDesc.label = wgpu::StringView("RendererWGPU::drawShot encoder");
    currentEncoder = ctx.device()->createCommandEncoder(encDesc);

    wgpu::RenderPassColorAttachment colorAtt{};
    colorAtt.view = targetView;
    colorAtt.loadOp = targetLoadOp;
    colorAtt.storeOp = wgpu::StoreOp::Store;
    colorAtt.clearValue = {33.0 / 255.0, 33.0 / 255.0, 33.0 / 255.0, 1.0};

    wgpu::RenderPassDepthStencilAttachment depthAtt{};
    depthAtt.view = depthView;
    depthAtt.depthLoadOp = targetLoadOp;
    depthAtt.depthStoreOp = wgpu::StoreOp::Store;
    depthAtt.depthClearValue = 1.0f;
    depthAtt.stencilLoadOp = wgpu::LoadOp::Undefined;
    depthAtt.stencilStoreOp = wgpu::StoreOp::Undefined;

    wgpu::RenderPassDescriptor passDesc{};
    passDesc.label = wgpu::StringView("RendererWGPU::drawShot pass");
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAtt;
    passDesc.depthStencilAttachment = &depthAtt;

    currentPass = currentEncoder->beginRenderPass(passDesc);
}

void RendererWGPU::endFrame() {
    if (!currentPass || !currentEncoder) {
        return;
    }

    currentPass->end();
    wgpu::raii::CommandBuffer cmd = currentEncoder->finish();
    WGPUContext::instance().queue()->submit(1, &*cmd);
    currentPass = wgpu::raii::RenderPassEncoder{};
    currentEncoder = wgpu::raii::CommandEncoder{};
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
    // transient-правке сцены (§6.2): CPU AtomStorage уже изменён, а резидентный
    // re-upload отложен (напр. на паузе), поэтому boundCount = min(count,
    // residency.boundCount) НИКОГДА не читает за пределами меньшего из буферов. В
    // CPU-режиме boundCount == count (поведение прежнее).
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
        else {
            // Auto-max: CPU-скан max-скорости по RenderAtomsView. В GPU-режиме это
            // читает CPU-скорости — их свежесть гарантирует условный per-frame sync
            // App-слоя: предикат cpuPositionConsumerActive ВКЛЮЧАЕТ speed-color-auto-max,
            // поэтому при этой ветке download сделан и vx/vy/vz свежи. Если hasVelocities()
            // ложно (теоретически в GPU-режиме при отсутствии CPU-зеркала) — остаёмся на 1.0f.
            if (atoms.hasVelocities()) {
                const auto it = std::ranges::max_element(std::views::iota(size_t{0}, count), {}, [&](size_t i) {
                    return atoms.vx[i] * atoms.vx[i] + atoms.vy[i] * atoms.vy[i] + atoms.vz[i] * atoms.vz[i];
                });
                const float speedSqr = atoms.vx[*it] * atoms.vx[*it] + atoms.vy[*it] * atoms.vy[*it] + atoms.vz[*it] * atoms.vz[*it];
                maxSpeedSqr = std::max(1e-6f, speedSqr);
            }
        }
    }
    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, offsetof(SceneUniforms, maxSpeedSqr), &maxSpeedSqr, sizeof(float));

    currentPass->setPipeline(*atomPipeline);
    currentPass->setBindGroup(0, ensureAtomBindGroup(boundCount, residency), 0, nullptr);
    currentPass->setVertexBuffer(0, *atomQuadVb, 0, atomQuadVb->getSize());
    currentPass->draw(6, static_cast<uint32_t>(boundCount), 0, 0);
}

void RendererWGPU::drawBoxImpl(const glm::vec3& worldSize) {
    if (worldSize != cachedBoxSize_) {
        cachedBoxSize_ = worldSize;
        const float x1 = cachedBoxSize_.x;
        const float y1 = cachedBoxSize_.y;
        const float z1 = cachedBoxSize_.z;
        boxVertices_ = {
            0, y1, 0, x1, y1, 0, x1, y1, 0, x1, y1, z1, x1, y1, z1, 0,  y1, z1, 0, y1, z1, 0, y1, 0,
            0, 0,  0, x1, 0,  0, x1, 0,  0, x1, 0,  z1, x1, 0,  z1, 0,  0,  z1, 0, 0,  z1, 0, 0,  0,
            0, 0,  0, 0,  y1, 0, x1, 0,  0, x1, y1, 0,  x1, 0,  z1, x1, y1, z1, 0, 0,  z1, 0, y1, z1,
        };
        WGPUContext::instance().queue()->writeBuffer(*boxVb, 0, boxVertices_.data(), sizeof(boxVertices_));
    }

    currentPass->setPipeline(*boxPipeline);
    currentPass->setBindGroup(0, *lineBindGroup, 0, nullptr);
    currentPass->setVertexBuffer(0, *boxVb, 0, sizeof(boxVertices_));
    currentPass->draw(24, 1, 0, 0);
}

void RendererWGPU::setLineColor(const glm::vec4& color) {
    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, offsetof(SceneUniforms, lineColor), &color, sizeof(color));
}

void RendererWGPU::drawBondsImpl(const RenderAtomsView& atoms, const RenderBondsView& bonds) {
    if (bonds.empty()) {
        return;
    }

    struct BondVertexBuildContext {
        const RenderAtomsView* atoms = nullptr;
        std::vector<glm::vec3>* verts = nullptr;

        static void append(size_t aIndex, size_t bIndex, void* userData) {
            auto& ctx = *static_cast<BondVertexBuildContext*>(userData);
            const RenderAtomsView& atoms = *ctx.atoms;
            if (aIndex >= atoms.count || bIndex >= atoms.count || !atoms.hasPositions()) {
                return;
            }

            ctx.verts->emplace_back(atoms.x[aIndex], atoms.y[aIndex], atoms.z[aIndex]);
            ctx.verts->emplace_back(atoms.x[bIndex], atoms.y[bIndex], atoms.z[bIndex]);
        }
    };

    std::vector<glm::vec3> verts;
    verts.reserve(bonds.count * 2);
    BondVertexBuildContext buildContext{.atoms = &atoms, .verts = &verts};
    bonds.forEach(BondVertexBuildContext::append, &buildContext);
    if (verts.empty()) {
        return;
    }

    const uint64_t bytes = verts.size() * sizeof(glm::vec3);
    if (bytes > bondVbCapacity_) {
        bondVb = WGPUContext::instance().createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Bond_Geometry");
        bondVbCapacity_ = bytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*bondVb, 0, verts.data(), bytes);

    currentPass->setPipeline(*bondPipeline);
    currentPass->setBindGroup(0, *lineBindGroup, 0, nullptr);
    currentPass->setVertexBuffer(0, *bondVb, 0, bytes);
    currentPass->draw(verts.size(), 1, 0, 0);
}

void RendererWGPU::drawGridImpl(const RenderGridView& grid) {
    struct GridBuildContext {
        RendererWGPU* renderer = nullptr;
        float maxCount = 1.0f;

        static void append(const RenderGridCell& cell, void* userData) {
            auto& ctx = *static_cast<GridBuildContext*>(userData);
            ctx.renderer->gridData.push_back(GridInstance{
                .origin = glm::vec4(cell.origin, 0.0f),
                .cellSize = cell.cellSize,
                .atomCount = cell.atomCount,
            });
            ctx.maxCount = std::max(ctx.maxCount, cell.atomCount);
        }
    };

    gridData.clear();
    gridData.reserve(grid.count);
    GridBuildContext buildContext{.renderer = this};
    grid.forEach(GridBuildContext::append, &buildContext);
    if (gridData.empty()) {
        return;
    }

    float mc = buildContext.maxCount;
    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, offsetof(SceneUniforms, maxCount), &mc, sizeof(float));

    const uint64_t instBytes = gridData.size() * sizeof(GridInstance);
    if (!gridInstVb || instBytes > gridInstVbCapacity_) {
        gridInstVb =
            WGPUContext::instance().createBuffer(instBytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Grid_Instances");
        gridInstVbCapacity_ = instBytes * 2;
    }
    WGPUContext::instance().queue()->writeBuffer(*gridInstVb, 0, gridData.data(), instBytes);

    currentPass->setPipeline(*gridPipeline);
    currentPass->setBindGroup(0, *gridBindGroup, 0, nullptr);
    currentPass->setVertexBuffer(0, *gridLineVb, 0, gridLineVb->getSize());
    currentPass->setVertexBuffer(1, *gridInstVb, 0, instBytes);
    currentPass->draw(24, gridData.size(), 0, 0);
}
