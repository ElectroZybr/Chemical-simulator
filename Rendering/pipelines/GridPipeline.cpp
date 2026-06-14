#include "Rendering/Renderer.h"

#include <array>
#include <cstddef>
#include <string_view>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Rendering/pipelines/PipelineHelpers.h"
#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initGridPipeline(std::string_view gridWGSL) {
    wgpu::ShaderModule shader = Pipeline::createShaderModule(gridWGSL);

    wgpu::BindGroupLayoutEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    uboEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    gridLayer_.bindGroupLayout = WGPUContext::instance().createBindGroupLayout({&uboEntry, 1}, "GridBindGroupLayout");

    wgpu::PipelineLayout pipelineLayout = Pipeline::createPipelineLayout("GridPipelineLayout", *gridLayer_.bindGroupLayout);

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

    wgpu::BlendState blend = Pipeline::makeAlphaBlend();

    wgpu::ColorTargetState colorTarget = Pipeline::makeColorTargetState(surfaceFormat, &blend);
    wgpu::FragmentState fragState = Pipeline::makeFragmentState(shader, colorTarget);

    wgpu::DepthStencilState depthState = Pipeline::makeDepthState(wgpu::OptionalBool::False);
    pipelines_.grid = Pipeline::createRenderPipeline("GridRenderPipeline", pipelineLayout, shader, vbLayouts, fragState,
                                                     wgpu::PrimitiveTopology::LineList, &depthState);

    for (size_t i = 0; i < kGridUniformSlotCount; ++i) {
        gridLayer_.uniformBuffers[i] =
            WGPUContext::instance().createBuffer(sizeof(SceneUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "GridUniforms");

        wgpu::BindGroupEntry entry{};
        entry.binding = 0;
        entry.buffer = *gridLayer_.uniformBuffers[i];
        entry.size = sizeof(SceneUniforms);

        gridLayer_.bindGroups[i] = WGPUContext::instance().createBindGroup(*gridLayer_.bindGroupLayout, {&entry, 1}, "GridBindGroup");
    }
}
