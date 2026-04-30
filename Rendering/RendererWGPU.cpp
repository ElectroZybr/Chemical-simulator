#include "RendererWGPU.h"

#include <cstring>
#include <ranges>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "App/interaction/ToolsManager.h"

namespace {
    wgpu::ShaderModule createShaderModule(wgpu::Device device, const char* wgsl) {
        WGPUShaderSourceWGSL wgslDesc{};
        wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
        wgslDesc.code = {wgsl, WGPU_STRLEN};

        wgpu::ShaderModuleDescriptor desc{};
        desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
        return device.createShaderModule(desc);
    }

}

RendererWGPU::RendererWGPU(SimBox& simbox, wgpu::Device device, wgpu::TextureFormat surfaceFormat)
    : IRenderer(simbox), device(device), surfaceFormat(surfaceFormat) {
    wgpu::BufferDescriptor desc;
    desc.size = sizeof(SceneUniforms);
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniformBuffer = device.createBuffer(desc);

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

wgpu::Buffer RendererWGPU::createBuffer(uint64_t size, wgpu::BufferUsage usage, wgpu::StringView label) {
    wgpu::BufferDescriptor desc;
    desc.label = label;
    desc.size = size;
    desc.usage = usage;
    return device.createBuffer(desc);
}

void RendererWGPU::initAtomQuadBuffer() {
    static constexpr float quad[] = {
        -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
    };
    atomQuadVb = createBuffer(sizeof(quad), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, wgpu::StringView("Atom_Quad_Geometry"));
    device.getQueue().writeBuffer(atomQuadVb, 0, quad, sizeof(quad));
}

void RendererWGPU::initBoxBuffer() {
    boxVb = createBuffer(24 * 3 * sizeof(float), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, wgpu::StringView("Box_Geometry"));
}

void RendererWGPU::initBondBuffer() {
    bondVb = createBuffer(128, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst);
    bondVbCapacity_ = 128;
}

void RendererWGPU::initGridLineBuffer() {
    const float lines[] = {
        0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
        1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1,
    };
    gridLineVb =
        createBuffer(sizeof(lines), wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, wgpu::StringView("Grid_Cell_Unit_Lines"));
    device.getQueue().writeBuffer(gridLineVb, 0, lines, sizeof(lines));
}

void RendererWGPU::initAtomPipeline(const char* atomWGSL) {
    wgpu::ShaderModule shader = createShaderModule(device, atomWGSL);

    std::array<wgpu::BindGroupLayoutEntry, 6> entries;
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    entries[0].buffer.type = wgpu::BufferBindingType::Uniform;

    for (int i = 1; i <= 5; ++i) {
        entries[i].binding = i;
        entries[i].visibility = wgpu::ShaderStage::Vertex;
        entries[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    }

    wgpu::BindGroupLayoutDescriptor bglDesc{};
    bglDesc.label = wgpu::StringView("AtomBindGroupLayout");
    bglDesc.entryCount = entries.size();
    bglDesc.entries = entries.data();
    atomBindGroupLayout = device.createBindGroupLayout(bglDesc);

    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView("AtomPipelineLayout");
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&atomBindGroupLayout;
    wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(plDesc);

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

    atomPipeline = device.createRenderPipeline(pDesc);
}

void RendererWGPU::initLinePipeline(wgpu::RenderPipeline& outPipeline, const char* wgsl) {
    wgpu::ShaderModule shader = createShaderModule(device, wgsl);

    wgpu::BindGroupLayoutEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.visibility = wgpu::ShaderStage::Vertex;
    uboEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    wgpu::BindGroupLayoutDescriptor bglDesc{};
    bglDesc.label = wgpu::StringView("LineBindGroupLayout");
    bglDesc.entryCount = 1;
    bglDesc.entries = &uboEntry;
    wgpu::BindGroupLayout bgl = device.createBindGroupLayout(bglDesc);

    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView("LinePipelineLayout");
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bgl;

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
    pDesc.layout = device.createPipelineLayout(plDesc);
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

    outPipeline = device.createRenderPipeline(pDesc);

    lineBindGroupLayout = bgl;

    wgpu::BindGroupEntry entry{};
    entry.binding = 0;
    entry.buffer = uniformBuffer;
    entry.size = sizeof(SceneUniforms);

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = lineBindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &entry;
    lineBindGroup = device.createBindGroup(bgDesc);
}

void RendererWGPU::initGridPipeline(const char* gridWGSL) {
    wgpu::ShaderModule shader = createShaderModule(device, gridWGSL);

    wgpu::BindGroupLayoutEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.visibility = wgpu::ShaderStage::Vertex;
    uboEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    wgpu::BindGroupLayoutDescriptor bglDesc{};
    bglDesc.label = wgpu::StringView("GridBindGroupLayout");
    bglDesc.entryCount = 1;
    bglDesc.entries = &uboEntry;
    wgpu::BindGroupLayout bgl = device.createBindGroupLayout(bglDesc);

    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView("GridPipelineLayout");
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bgl;

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
    pDesc.layout = device.createPipelineLayout(plDesc);
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

    gridPipeline = device.createRenderPipeline(pDesc);
    gridBindGroupLayout = bgl;

    wgpu::BindGroupEntry entry{};
    entry.binding = 0;
    entry.buffer = uniformBuffer;
    entry.size = sizeof(SceneUniforms);

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = gridBindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &entry;
    gridBindGroup = device.createBindGroup(bgDesc);
}

void RendererWGPU::initBoxPipeline(const char* boxWGSL) { initLinePipeline(boxPipeline, boxWGSL); }
void RendererWGPU::initBondPipeline(const char* bondWGSL) { initLinePipeline(bondPipeline, bondWGSL); }

void RendererWGPU::ensureStorageBuffers(size_t count) {
    if (count <= sbCapacity_) {
        return;
    }

    const uint64_t vec4Bytes = count * sizeof(AtomVec4);
    const uint64_t f32Bytes = count * sizeof(float);
    const wgpu::BufferUsage usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;

    sbPos = createBuffer(vec4Bytes, usage, wgpu::StringView("Atoms_Pos"));
    sbVel = createBuffer(vec4Bytes, usage, wgpu::StringView("Atoms_Vel"));
    sbType = createBuffer(f32Bytes, usage, wgpu::StringView("Atoms_Type"));
    sbRadius = createBuffer(f32Bytes, usage, wgpu::StringView("Atoms_Radius"));
    sbSel = createBuffer(f32Bytes, usage, wgpu::StringView("Atoms_Selection"));
    sbCapacity_ = count;

    std::array<wgpu::BindGroupEntry, 6> entries{};
    entries[0].binding = 0;
    entries[0].buffer = uniformBuffer;
    entries[0].size = sizeof(SceneUniforms);
    entries[1].binding = 1;
    entries[1].buffer = sbPos;
    entries[1].size = vec4Bytes;
    entries[2].binding = 2;
    entries[2].buffer = sbVel;
    entries[2].size = vec4Bytes;
    entries[3].binding = 3;
    entries[3].buffer = sbType;
    entries[3].size = f32Bytes;
    entries[4].binding = 4;
    entries[4].buffer = sbRadius;
    entries[4].size = f32Bytes;
    entries[5].binding = 5;
    entries[5].buffer = sbSel;
    entries[5].size = f32Bytes;

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.label = wgpu::StringView("AtomBindGroup");
    bgDesc.layout = atomBindGroupLayout;
    bgDesc.entryCount = entries.size();
    bgDesc.entries = entries.data();
    atomBindGroup = device.createBindGroup(bgDesc);
}

template <typename T> void RendererWGPU::uploadStorageBuffer(wgpu::Buffer& buf, const T* data, size_t count) {
    device.getQueue().writeBuffer(buf, 0, data, count * sizeof(T));
}

void RendererWGPU::drawShot(wgpu::TextureView targetView, wgpu::TextureView depthView, const AtomStorage& atoms, const Bond::List& bonds,
                            const SimBox& box) {
    currentTargetView = targetView;
    currentDepthView = depthView;
    updateMatrices();

    SceneUniforms uniforms{};
    uniforms.view = view;
    uniforms.projection = projection;
    uniforms.lightDir = glm::vec4(getLightDir(), 0.f);
    uniforms.colorMode = glm::vec4(static_cast<float>(speedColorMode), 0, 0, 0);
    uniforms.maxSpeedSqr = glm::vec4(1.f, 0, 0, 0);
    uniforms.maxCount = glm::vec4(1.f, 0, 0, 0);
    for (size_t i = 0; i < typeColorsData.size(); ++i) {
        uniforms.typeColors[i] = typeColorsData[i];
    }

    device.getQueue().writeBuffer(uniformBuffer, 0, &uniforms, sizeof(uniforms));

    wgpu::CommandEncoderDescriptor encDesc;
    encDesc.label = wgpu::StringView("RendererWGPU::drawShot encoder");
    currentEncoder = device.createCommandEncoder(encDesc);

    wgpu::RenderPassColorAttachment colorAtt;
    colorAtt.view = currentTargetView;
    colorAtt.loadOp = wgpu::LoadOp::Clear;
    colorAtt.storeOp = wgpu::StoreOp::Store;
    colorAtt.clearValue = {33.0 / 255.0, 33.0 / 255.0, 33.0 / 255.0, 1.0};

    wgpu::RenderPassDepthStencilAttachment depthAtt;
    depthAtt.view = currentDepthView;
    depthAtt.depthLoadOp = wgpu::LoadOp::Clear;
    depthAtt.depthStoreOp = wgpu::StoreOp::Store;
    depthAtt.depthClearValue = 1.0f;
    depthAtt.stencilLoadOp = wgpu::LoadOp::Undefined;
    depthAtt.stencilStoreOp = wgpu::StoreOp::Undefined;

    wgpu::RenderPassDescriptor passDesc;
    passDesc.label = wgpu::StringView("RendererWGPU::drawShot pass");
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAtt;
    passDesc.depthStencilAttachment = &depthAtt;

    currentPass = currentEncoder.beginRenderPass(passDesc);

    if (drawBonds) {
        drawBondsImpl(atoms, bonds);
    }
    if (drawGrid) {
        drawGridImpl(box.grid);
    }
    drawBoxImpl(box);
    drawAtomsImpl(atoms);
}

void RendererWGPU::endFrame() {
    currentPass.end();
    auto cmd = currentEncoder.finish();
    device.getQueue().submit(1, &cmd);
    currentEncoder = nullptr;
    currentPass = nullptr;
}

void RendererWGPU::drawAtomsImpl(const AtomStorage& atoms) {
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
    for (const size_t idx : ToolsManager::pickingSystem->getSelectedIndices()) {
        if (idx < count) {
            selectedData[idx] = 1.0f;
        }
    }

    uploadStorageBuffer(sbPos, posData_.data(), count);
    uploadStorageBuffer(sbVel, velData_.data(), count);
    uploadStorageBuffer(sbRadius, radii.data(), count);
    uploadStorageBuffer(sbType, typeData.data(), count);
    uploadStorageBuffer(sbSel, selectedData.data(), count);

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
    device.getQueue().writeBuffer(uniformBuffer, offsetof(SceneUniforms, maxSpeedSqr), &maxSpeedSqr, sizeof(float));

    currentPass.setPipeline(atomPipeline);
    currentPass.setBindGroup(0, atomBindGroup, 0, nullptr);
    currentPass.setVertexBuffer(0, atomQuadVb, 0, atomQuadVb.getSize());
    currentPass.draw(6, count, 0, 0);
}

void RendererWGPU::drawBoxImpl(const SimBox& box) {
    if (box.size.x != cachedBoxSize_.x || box.size.y != cachedBoxSize_.y || box.size.z != cachedBoxSize_.z) {
        cachedBoxSize_ = box.size;
        const float x1 = box.size.x;
        const float y1 = box.size.y;
        const float z1 = box.size.z;
        boxVertices_ = {
            0, y1, 0, x1, y1, 0, x1, y1, 0, x1, y1, z1, x1, y1, z1, 0,  y1, z1, 0, y1, z1, 0, y1, 0,
            0, 0,  0, x1, 0,  0, x1, 0,  0, x1, 0,  z1, x1, 0,  z1, 0,  0,  z1, 0, 0,  z1, 0, 0,  0,
            0, 0,  0, 0,  y1, 0, x1, 0,  0, x1, y1, 0,  x1, 0,  z1, x1, y1, z1, 0, 0,  z1, 0, y1, z1,
        };
        device.getQueue().writeBuffer(boxVb, 0, boxVertices_.data(), sizeof(boxVertices_));
    }

    currentPass.setPipeline(boxPipeline);
    currentPass.setBindGroup(0, lineBindGroup, 0, nullptr);
    currentPass.setVertexBuffer(0, boxVb, 0, sizeof(boxVertices_));
    currentPass.draw(24, 1, 0, 0);
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
        bondVb = createBuffer(bytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst);
        bondVbCapacity_ = bytes * 2;
    }
    device.getQueue().writeBuffer(bondVb, 0, verts.data(), bytes);

    currentPass.setPipeline(bondPipeline);
    currentPass.setBindGroup(0, lineBindGroup, 0, nullptr);
    currentPass.setVertexBuffer(0, bondVb, 0, bytes);
    currentPass.draw(verts.size(), 1, 0, 0);
}

void RendererWGPU::drawGridImpl(const SpatialGrid& grid) {
    gridData.clear();
    int maxCount = 1;

    for (int z = 1; z < grid.sizeZ - 1; ++z) {
        for (int y = 1; y < grid.sizeY - 1; ++y) {
            for (int x = 1; x < grid.sizeX - 1; ++x) {
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
    device.getQueue().writeBuffer(uniformBuffer, offsetof(SceneUniforms, maxCount), &mc, sizeof(float));

    const uint64_t instBytes = gridData.size() * sizeof(GridInstance);
    if (!gridInstVb || instBytes > gridInstVbCapacity_) {
        gridInstVb = createBuffer(instBytes * 2, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst);
        gridInstVbCapacity_ = instBytes * 2;
    }
    device.getQueue().writeBuffer(gridInstVb, 0, gridData.data(), instBytes);

    currentPass.setPipeline(gridPipeline);
    currentPass.setBindGroup(0, gridBindGroup, 0, nullptr);
    currentPass.setVertexBuffer(0, gridLineVb, 0, gridLineVb.getSize());
    currentPass.setVertexBuffer(1, gridInstVb, 0, instBytes);
    currentPass.draw(24, gridData.size(), 0, 0);
}
