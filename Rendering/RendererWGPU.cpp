#include "RendererWGPU.h"

#include <cstring>
#include <ranges>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "App/interaction/ToolsManager.h"
#include "Engine/Simulation.h"
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

RendererWGPU::RendererWGPU(World& world, wgpu::TextureFormat surfaceFormat) : IRenderer(world), surfaceFormat(surfaceFormat) {
    uniformBuffer = WGPUContext::instance().createBuffer(sizeof(SceneUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                         "RenderingUniforms");

    initAtomColors();
    initAtomQuadBuffer();
    initBoxBuffer();
    initBondBuffer();
    initGridLineBuffer();
}

void RendererWGPU::initAtomColors() {
    const int typeCount = static_cast<int>(AtomData::Type::COUNT);
    typeColorsData.resize(typeCount);
    for (int i = 0; i < typeCount; ++i) {
        const auto& props = AtomData::getProps(static_cast<AtomData::Type>(i));
        typeColorsData[i] = glm::vec4(props.color.r / 255.f, props.color.g / 255.f, props.color.b / 255.f, props.color.a / 255.f);
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

void RendererWGPU::drawShot(wgpu::TextureView targetView, wgpu::TextureView depthView, const Simulation& simulation) {
    if (simulation.worldCount() == 0) {
        beginPass(targetView, depthView, wgpu::LoadOp::Clear);
        return;
    }

    for (Simulation::WorldId worldId = 0; worldId < simulation.worldCount(); ++worldId) {
        drawWorldPass(targetView, depthView, simulation.worldAt(worldId), worldId == 0 ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
                      worldId == simulation.activeWorldId());
        if (worldId + 1 < simulation.worldCount()) {
            endFrame();
        }
    }
}

void RendererWGPU::drawWorldPass(wgpu::TextureView targetView, wgpu::TextureView depthView, const World& world, wgpu::LoadOp targetLoadOp,
                                 bool applySelection) {
    updateMatrices();

    SceneUniforms uniforms{};
    uniforms.view = view;
    uniforms.projection = projection;
    uniforms.lightDir = glm::vec4(getLightDir(), 0.f);
    uniforms.colorMode = glm::vec4(static_cast<float>(speedColorMode), 0, 0, 0);
    uniforms.maxSpeedSqr = glm::vec4(1.f, 0, 0, 0);
    uniforms.maxCount = glm::vec4(1.f, 0, 0, 0);
    uniforms.renderOffset = glm::vec4(world.getRenderOffset().x, world.getRenderOffset().y, world.getRenderOffset().z, 0.0f);
    uniforms.lineColor = glm::vec4(0.4f, 0.6f, 1.0f, 0.3f);
    for (size_t i = 0; i < typeColorsData.size(); ++i) {
        uniforms.typeColors[i] = typeColorsData[i];
    }

    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, 0, &uniforms, sizeof(uniforms));

    beginPass(targetView, depthView, targetLoadOp);

    if (drawBonds) {
        setLineColor(glm::vec4(0.4f, 0.6f, 1.0f, 0.3f));
        drawBondsImpl(world.getAtomStorage(), world.getBonds());
    }
    if (drawGrid) {
        drawGridImpl(world.getGrid());
    }
    if (drawBox) {
        setLineColor(applySelection ? glm::vec4(0.5f, 0.78f, 1.0f, 0.55f) : glm::vec4(0.35f, 0.52f, 0.9f, 0.3f));
        drawBoxImpl(world.getWorldSize());
    }
    drawAtomsImpl(world.getAtomStorage(), applySelection);
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

void RendererWGPU::drawAtomsImpl(const AtomStorage& atoms, bool applySelection) {
    const size_t count = atoms.size();
    if (count == 0) {
        return;
    }

    ensureStorageBuffers(count);

    posData_.resize(count);
    velData_.resize(count);
    radii.resize(count);
    typeData.resize(count);
    selectedData.assign(count, 0.0f);

    for (size_t i = 0; i < count; ++i) {
        posData_[i] = {atoms.xData()[i], atoms.yData()[i], atoms.zData()[i]};
        velData_[i] = {atoms.vxData()[i], atoms.vyData()[i], atoms.vzData()[i]};
        const auto& props = AtomData::getProps(atoms.type(i));
        radii[i] = props.radius;
        typeData[i] = static_cast<float>(atoms.type(i));
    }
    if (applySelection && ToolsManager::pickingSystem != nullptr) {
        for (const size_t idx : ToolsManager::pickingSystem->getSelectedIndices()) {
            if (idx < count) {
                selectedData[idx] = 1.0f;
            }
        }
    }

    uploadStorageBuffer(*sbPos, posData_.data(), count);
    uploadStorageBuffer(*sbVel, velData_.data(), count);
    uploadStorageBuffer(*sbRadius, radii.data(), count);
    uploadStorageBuffer(*sbType, typeData.data(), count);
    uploadStorageBuffer(*sbSel, selectedData.data(), count);

    float maxSpeedSqr = 1.f;
    if (speedColorMode != SpeedColorMode::AtomColor) {
        if (speedGradientMax > 0.f) {
            maxSpeedSqr = speedGradientMax * speedGradientMax;
        }
        else {
            const auto it =
                std::ranges::max_element(std::views::iota(size_t{0}, count), {}, [&](size_t i) { return atoms.vel(i).sqrAbs(); });
            maxSpeedSqr = std::max(1e-6f, atoms.vel(*it).sqrAbs());
        }
    }
    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, offsetof(SceneUniforms, maxSpeedSqr), &maxSpeedSqr, sizeof(float));

    currentPass->setPipeline(*atomPipeline);
    currentPass->setBindGroup(0, *atomBindGroup, 0, nullptr);
    currentPass->setVertexBuffer(0, *atomQuadVb, 0, atomQuadVb->getSize());
    currentPass->draw(6, count, 0, 0);
}

void RendererWGPU::drawBoxImpl(const Vec3f& worldSize) {
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

void RendererWGPU::drawBondsImpl(const AtomStorage& atoms, const Bond::List& bonds) {
    if (bonds.empty()) {
        return;
    }

    std::vector<glm::vec3> verts;
    verts.reserve(bonds.size() * 2);
    for (const Bond& bond : bonds) {
        if (bond.aIndex >= atoms.size() || bond.bIndex >= atoms.size()) {
            continue;
        }
        const Vec3f a = atoms.pos(bond.aIndex);
        const Vec3f b = atoms.pos(bond.bIndex);
        verts.emplace_back(a.x, a.y, a.z);
        verts.emplace_back(b.x, b.y, b.z);
    }
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

void RendererWGPU::drawGridImpl(const SpatialGrid& grid) {
    gridData.clear();
    int maxCount = 1;

    for (unsigned int z = 1; z < grid.size.z - 1; ++z) {
        for (unsigned int y = 1; y < grid.size.y - 1; ++y) {
            for (unsigned int x = 1; x < grid.size.x - 1; ++x) {
                const int cnt = grid.countAtomsInCell(x, y, z);
                if (cnt > 0) {
                    gridData.emplace_back(glm::vec4((x - 1) * grid.cellSize, (y - 1) * grid.cellSize, (z - 1) * grid.cellSize, 0.f),
                                          (float)grid.cellSize, (float)cnt);
                    maxCount = std::max(maxCount, cnt);
                }
            }
        }
    }
    if (gridData.empty()) {
        return;
    }

    float mc = (float)maxCount;
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
