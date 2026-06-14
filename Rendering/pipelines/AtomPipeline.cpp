#include "Rendering/Renderer.h"

#include <array>
#include <string_view>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Rendering/pipelines/PipelineHelpers.h"
#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initAtomSpherePipeline(std::string_view atomWGSL) {
    wgpu::ShaderModule shader = Pipeline::createShaderModule(atomWGSL);

    std::array<wgpu::BindGroupLayoutEntry, 6> entries;
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    entries[0].buffer.type = wgpu::BufferBindingType::Uniform;

    for (int i = 1; i <= 5; ++i) {
        entries[i].binding = i;
        entries[i].visibility = wgpu::ShaderStage::Vertex;
        entries[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    }

    atomLayer_.bindGroupLayout = WGPUContext::instance().createBindGroupLayout(entries, "AtomBindGroupLayout");

    wgpu::PipelineLayout pipelineLayout = Pipeline::createPipelineLayout("AtomPipelineLayout", *atomLayer_.bindGroupLayout);

    wgpu::VertexAttribute quadAttr{};
    quadAttr.format = wgpu::VertexFormat::Float32x2;
    quadAttr.offset = 0;
    quadAttr.shaderLocation = 0;

    wgpu::VertexBufferLayout quadLayout{};
    quadLayout.arrayStride = 2 * sizeof(float);
    quadLayout.stepMode = wgpu::VertexStepMode::Vertex;
    quadLayout.attributeCount = 1;
    quadLayout.attributes = &quadAttr;

    wgpu::BlendState blend = Pipeline::makeAlphaBlend();
    wgpu::ColorTargetState colorTarget = Pipeline::makeColorTargetState(surfaceFormat, &blend);
    wgpu::FragmentState fragState = Pipeline::makeFragmentState(shader, colorTarget);

    wgpu::DepthStencilState depthState = Pipeline::makeDepthState(wgpu::OptionalBool::True);
    const std::array<wgpu::VertexBufferLayout, 1> layouts = {quadLayout};
    pipelines_.atomSphere = Pipeline::createRenderPipeline("AtomPipeline", pipelineLayout, shader, layouts, fragState,
                                                           wgpu::PrimitiveTopology::TriangleList, &depthState);
}
