#include "Render.h"

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Rendering/backend/WGPUContext.h"

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

    for (size_t i = 0; i < kLineUniformSlotCount; ++i) {
        lineUniformBuffers[i] =
            WGPUContext::instance().createBuffer(sizeof(SceneUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "LineUniforms");

        wgpu::BindGroupEntry entry{};
        entry.binding = 0;
        entry.buffer = *lineUniformBuffers[i];
        entry.size = sizeof(SceneUniforms);

        lineBindGroups[i] = WGPUContext::instance().createBindGroup(*lineBindGroupLayout, {&entry, 1}, "LineBindGroup");
    }
}

void RendererWGPU::initMemoryOrderPipeline(std::string_view memoryOrderWGSL) {
    wgpu::ShaderModule shader = createShaderModule(memoryOrderWGSL);

    wgpu::BindGroupLayoutEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.visibility = wgpu::ShaderStage::Vertex;
    uboEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    if (!lineBindGroupLayout) {
        lineBindGroupLayout = WGPUContext::instance().createBindGroupLayout({&uboEntry, 1}, "LineBindGroupLayout");
    }

    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView("MemoryOrderPipelineLayout");
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&lineBindGroupLayout;

    std::array<wgpu::VertexAttribute, 2> attrs{};
    attrs[0].format = wgpu::VertexFormat::Float32x3;
    attrs[0].offset = offsetof(MemoryOrderVertex, pos);
    attrs[0].shaderLocation = 0;
    attrs[1].format = wgpu::VertexFormat::Float32;
    attrs[1].offset = offsetof(MemoryOrderVertex, t);
    attrs[1].shaderLocation = 1;

    wgpu::VertexBufferLayout vbl{};
    vbl.arrayStride = sizeof(MemoryOrderVertex);
    vbl.stepMode = wgpu::VertexStepMode::Vertex;
    vbl.attributeCount = attrs.size();
    vbl.attributes = attrs.data();

    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = surfaceFormat;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragState{};
    fragState.module = shader;
    fragState.entryPoint = wgpu::StringView("fs_main");
    fragState.targetCount = 1;
    fragState.targets = &colorTarget;

    wgpu::RenderPipelineDescriptor pDesc{};
    pDesc.label = wgpu::StringView("MemoryOrderRenderPipeline");
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

    memoryOrderPipeline = WGPUContext::instance().device()->createRenderPipeline(pDesc);
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
