#include <array>
#include <string_view>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Rendering/pipelines/PipelineHelpers.h"
#include "Rendering/Renderer.h"
#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initPotentialFieldPipeline(std::string_view potentialFieldWGSL) {
    RendererWGPU& renderer = *this;
    wgpu::ShaderModule shader = Pipeline::createShaderModule(potentialFieldWGSL);

    wgpu::VertexAttribute vertAttr{};
    vertAttr.format = wgpu::VertexFormat::Float32x2;
    vertAttr.offset = 0;
    vertAttr.shaderLocation = 0;

    wgpu::VertexBufferLayout vertLayout{};
    vertLayout.arrayStride = 2 * sizeof(float);
    vertLayout.stepMode = wgpu::VertexStepMode::Vertex;
    vertLayout.attributeCount = 1;
    vertLayout.attributes = &vertAttr;

    std::array<wgpu::VertexAttribute, 3> instAttrs{};
    instAttrs[0].format = wgpu::VertexFormat::Float32x4;
    instAttrs[0].offset = offsetof(FieldInstance, origin);
    instAttrs[0].shaderLocation = 1;
    instAttrs[1].format = wgpu::VertexFormat::Float32x2;
    instAttrs[1].offset = offsetof(FieldInstance, cellSize);
    instAttrs[1].shaderLocation = 2;
    instAttrs[2].format = wgpu::VertexFormat::Float32x4;
    instAttrs[2].offset = offsetof(FieldInstance, potentials);
    instAttrs[2].shaderLocation = 3;

    wgpu::VertexBufferLayout instLayout{};
    instLayout.arrayStride = sizeof(FieldInstance);
    instLayout.stepMode = wgpu::VertexStepMode::Instance;
    instLayout.attributeCount = instAttrs.size();
    instLayout.attributes = instAttrs.data();

    std::array<wgpu::VertexBufferLayout, 2> vbLayouts = {vertLayout, instLayout};

    wgpu::BlendState blend = Pipeline::makeAlphaBlend();
    wgpu::ColorTargetState colorTarget = Pipeline::makeColorTargetState(renderer.surfaceFormat, &blend);
    wgpu::FragmentState fragState = Pipeline::makeFragmentState(shader, colorTarget);

    wgpu::DepthStencilState depthState = Pipeline::makeDepthState(wgpu::OptionalBool::False);
    wgpu::PipelineLayout pipelineLayout =
        Pipeline::createPipelineLayout("PotentialFieldPipelineLayout", *renderer.gridLayer_.bindGroupLayout);
    renderer.pipelines_.potentialField = Pipeline::createRenderPipeline("PotentialFieldRenderPipeline", pipelineLayout, shader, vbLayouts,
                                                                        fragState, wgpu::PrimitiveTopology::TriangleList, &depthState);
}

void RendererWGPU::initFieldContourPipeline(std::string_view fieldContourWGSL) {
    RendererWGPU& renderer = *this;
    wgpu::ShaderModule shader = Pipeline::createShaderModule(fieldContourWGSL);

    wgpu::VertexAttribute vertAttr{};
    vertAttr.format = wgpu::VertexFormat::Float32x2;
    vertAttr.offset = 0;
    vertAttr.shaderLocation = 0;

    wgpu::VertexBufferLayout vertLayout{};
    vertLayout.arrayStride = 2 * sizeof(float);
    vertLayout.stepMode = wgpu::VertexStepMode::Vertex;
    vertLayout.attributeCount = 1;
    vertLayout.attributes = &vertAttr;

    std::array<wgpu::VertexAttribute, 3> instAttrs{};
    instAttrs[0].format = wgpu::VertexFormat::Float32x4;
    instAttrs[0].offset = offsetof(FieldInstance, origin);
    instAttrs[0].shaderLocation = 1;
    instAttrs[1].format = wgpu::VertexFormat::Float32x2;
    instAttrs[1].offset = offsetof(FieldInstance, cellSize);
    instAttrs[1].shaderLocation = 2;
    instAttrs[2].format = wgpu::VertexFormat::Float32x4;
    instAttrs[2].offset = offsetof(FieldInstance, potentials);
    instAttrs[2].shaderLocation = 3;

    wgpu::VertexBufferLayout instLayout{};
    instLayout.arrayStride = sizeof(FieldInstance);
    instLayout.stepMode = wgpu::VertexStepMode::Instance;
    instLayout.attributeCount = instAttrs.size();
    instLayout.attributes = instAttrs.data();

    std::array<wgpu::VertexBufferLayout, 2> vbLayouts = {vertLayout, instLayout};

    wgpu::BlendState blend = Pipeline::makeAlphaBlend();
    wgpu::ColorTargetState colorTarget = Pipeline::makeColorTargetState(renderer.surfaceFormat, &blend);
    wgpu::FragmentState fragState = Pipeline::makeFragmentState(shader, colorTarget);

    wgpu::DepthStencilState depthState = Pipeline::makeDepthState(wgpu::OptionalBool::False);
    wgpu::PipelineLayout pipelineLayout =
        Pipeline::createPipelineLayout("FieldContourPipelineLayout", *renderer.gridLayer_.bindGroupLayout);
    renderer.pipelines_.fieldContours = Pipeline::createRenderPipeline("FieldContourRenderPipeline", pipelineLayout, shader, vbLayouts,
                                                                       fragState, wgpu::PrimitiveTopology::TriangleList, &depthState);
}

void RendererWGPU::initFieldArrowPipeline(std::string_view fieldArrowWGSL) {
    RendererWGPU& renderer = *this;
    wgpu::ShaderModule shader = Pipeline::createShaderModule(fieldArrowWGSL);

    wgpu::VertexAttribute vectorAttr{};
    vectorAttr.format = wgpu::VertexFormat::Float32x2;
    vectorAttr.offset = 0;
    vectorAttr.shaderLocation = 0;

    wgpu::VertexBufferLayout vectorLayout{};
    vectorLayout.arrayStride = sizeof(glm::vec2);
    vectorLayout.stepMode = wgpu::VertexStepMode::Instance;
    vectorLayout.attributeCount = 1;
    vectorLayout.attributes = &vectorAttr;

    wgpu::BlendState blend = Pipeline::makeAlphaBlend();
    wgpu::ColorTargetState colorTarget = Pipeline::makeColorTargetState(renderer.surfaceFormat, &blend);
    wgpu::FragmentState fragState = Pipeline::makeFragmentState(shader, colorTarget);

    wgpu::DepthStencilState depthState = Pipeline::makeDepthState(wgpu::OptionalBool::False);
    wgpu::PipelineLayout pipelineLayout =
        Pipeline::createPipelineLayout("FieldArrowPipelineLayout", *renderer.lineLayer_.bindGroupLayout);
    const std::array<wgpu::VertexBufferLayout, 1> layouts = {vectorLayout};
    renderer.pipelines_.fieldArrows = Pipeline::createRenderPipeline("FieldArrowRenderPipeline", pipelineLayout, shader, layouts, fragState,
                                                                     wgpu::PrimitiveTopology::LineList, &depthState);
}
