#pragma once

#include <span>
#include <string_view>

#include <webgpu/webgpu.hpp>

#include "Rendering/backend/WGPUContext.h"

#ifdef True
#undef True
#endif

#ifdef False
#undef False
#endif

namespace Pipeline {
inline wgpu::ShaderModule createShaderModule(std::string_view wgsl) {
    WGPUShaderSourceWGSL wgslDesc{};
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = wgpu::StringView(wgsl);

    wgpu::ShaderModuleDescriptor desc{};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
    return WGPUContext::instance().device()->createShaderModule(desc);
}

inline wgpu::BlendState makeAlphaBlend() {
    wgpu::BlendState blend{};
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha = blend.color;
    return blend;
}

inline wgpu::DepthStencilState makeDepthState(wgpu::OptionalBool depthWriteEnabled) {
    wgpu::DepthStencilState depthState{};
    depthState.format = wgpu::TextureFormat::Depth24Plus;
    depthState.depthWriteEnabled = depthWriteEnabled;
    depthState.depthCompare = wgpu::CompareFunction::Less;
    return depthState;
}

inline wgpu::ColorTargetState makeColorTargetState(wgpu::TextureFormat format, wgpu::BlendState* blend = nullptr) {
    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = format;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;
    colorTarget.blend = blend;
    return colorTarget;
}

inline wgpu::FragmentState makeFragmentState(wgpu::ShaderModule shader, wgpu::ColorTargetState& colorTarget,
                                             std::string_view entryPoint = "fs_main") {
    wgpu::FragmentState fragState{};
    fragState.module = shader;
    fragState.entryPoint = wgpu::StringView(entryPoint);
    fragState.targetCount = 1;
    fragState.targets = &colorTarget;
    return fragState;
}

inline wgpu::PipelineLayout createPipelineLayout(std::string_view label, WGPUBindGroupLayout bindGroupLayout) {
    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = wgpu::StringView(label);
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bindGroupLayout;
    return WGPUContext::instance().device()->createPipelineLayout(plDesc);
}

inline wgpu::RenderPipeline createRenderPipeline(std::string_view label, wgpu::PipelineLayout layout, wgpu::ShaderModule shader,
                                                 std::span<const wgpu::VertexBufferLayout> vertexLayouts, wgpu::FragmentState& fragmentState,
                                                 wgpu::PrimitiveTopology topology, wgpu::DepthStencilState* depthState,
                                                 std::string_view vertexEntryPoint = "vs_main") {
    wgpu::RenderPipelineDescriptor pDesc{};
    pDesc.label = wgpu::StringView(label);
    pDesc.layout = layout;
    pDesc.vertex.module = shader;
    pDesc.vertex.entryPoint = wgpu::StringView(vertexEntryPoint);
    pDesc.vertex.bufferCount = vertexLayouts.size();
    pDesc.vertex.buffers = vertexLayouts.data();
    pDesc.fragment = &fragmentState;
    pDesc.primitive.topology = topology;
    pDesc.multisample.count = 1;
    pDesc.multisample.mask = 0xFFFFFFFF;
    pDesc.multisample.alphaToCoverageEnabled = false;
    pDesc.depthStencil = depthState;
    return WGPUContext::instance().device()->createRenderPipeline(pDesc);
}

} // namespace Pipeline
