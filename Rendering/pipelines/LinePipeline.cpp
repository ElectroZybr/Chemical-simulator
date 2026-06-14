#include <array>
#include <cstddef>
#include <string_view>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Rendering/pipelines/PipelineHelpers.h"
#include "Rendering/Renderer.h"
#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initLinePipeline(wgpu::RenderPipeline& outPipeline, std::string_view wgsl) {
    RendererWGPU& renderer = *this;
    wgpu::ShaderModule shader = Pipeline::createShaderModule(wgsl);

    wgpu::BindGroupLayoutEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    uboEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    renderer.lineLayer_.bindGroupLayout = WGPUContext::instance().createBindGroupLayout({&uboEntry, 1}, "LineBindGroupLayout");

    wgpu::PipelineLayout pipelineLayout = Pipeline::createPipelineLayout("LinePipelineLayout", *renderer.lineLayer_.bindGroupLayout);

    wgpu::VertexAttribute attr{};
    attr.format = wgpu::VertexFormat::Float32x3;
    attr.offset = 0;
    attr.shaderLocation = 0;

    wgpu::VertexBufferLayout vbl{};
    vbl.arrayStride = 3 * sizeof(float);
    vbl.stepMode = wgpu::VertexStepMode::Vertex;
    vbl.attributeCount = 1;
    vbl.attributes = &attr;

    wgpu::ColorTargetState colorTarget = Pipeline::makeColorTargetState(renderer.surfaceFormat);
    wgpu::FragmentState fragState = Pipeline::makeFragmentState(shader, colorTarget);

    wgpu::DepthStencilState depthState = Pipeline::makeDepthState(wgpu::OptionalBool::False);

    const std::array<wgpu::VertexBufferLayout, 1> layouts = {vbl};
    outPipeline = Pipeline::createRenderPipeline("LineRenderPipeline", pipelineLayout, shader, layouts, fragState,
                                                 wgpu::PrimitiveTopology::LineList, &depthState);

    for (size_t i = 0; i < RendererWGPU::kLineUniformSlotCount; ++i) {
        renderer.lineLayer_.uniformBuffers[i] =
            WGPUContext::instance().createBuffer(sizeof(RendererWGPU::SceneUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "LineUniforms");

        wgpu::BindGroupEntry entry{};
        entry.binding = 0;
        entry.buffer = *renderer.lineLayer_.uniformBuffers[i];
        entry.size = sizeof(RendererWGPU::SceneUniforms);

        renderer.lineLayer_.bindGroups[i] = WGPUContext::instance().createBindGroup(*renderer.lineLayer_.bindGroupLayout, {&entry, 1}, "LineBindGroup");
    }
}

void RendererWGPU::initMemoryOrderPipeline(std::string_view wgsl) {
    RendererWGPU& renderer = *this;
    wgpu::ShaderModule shader = Pipeline::createShaderModule(wgsl);

    wgpu::BindGroupLayoutEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.visibility = wgpu::ShaderStage::Vertex;
    uboEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    if (!renderer.lineLayer_.bindGroupLayout) {
        renderer.lineLayer_.bindGroupLayout = WGPUContext::instance().createBindGroupLayout({&uboEntry, 1}, "LineBindGroupLayout");
    }

    wgpu::PipelineLayout pipelineLayout = Pipeline::createPipelineLayout("MemoryOrderPipelineLayout", *renderer.lineLayer_.bindGroupLayout);

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

    wgpu::ColorTargetState colorTarget = Pipeline::makeColorTargetState(renderer.surfaceFormat);
    wgpu::FragmentState fragState = Pipeline::makeFragmentState(shader, colorTarget);

    wgpu::DepthStencilState depthState = Pipeline::makeDepthState(wgpu::OptionalBool::False);

    const std::array<wgpu::VertexBufferLayout, 1> layouts = {vbl};
    renderer.pipelines_.memoryOrder = Pipeline::createRenderPipeline("MemoryOrderRenderPipeline", pipelineLayout, shader, layouts, fragState, wgpu::PrimitiveTopology::LineList, &depthState);
}

void RendererWGPU::initBoxPipeline(std::string_view boxWGSL) { initLinePipeline(*pipelines_.box, boxWGSL); }

void RendererWGPU::initBondPipeline(std::string_view bondWGSL) { initLinePipeline(*pipelines_.bond, bondWGSL); }
