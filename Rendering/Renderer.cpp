#include "Renderer.h"

#include "Rendering/backend/WGPUContext.h"
#include "generated/shaders/arrows.wgsl.h"
#include "generated/shaders/contour.wgsl.h"
#include "generated/shaders/field.wgsl.h"
#include "generated/shaders/grid.wgsl.h"
#include "generated/shaders/line.wgsl.h"
#include "generated/shaders/memline.wgsl.h"

RendererWGPU::RendererWGPU() : surfaceFormat(WGPUContext::instance().surfaceFormat()) {
    uniformBuffer = WGPUContext::instance().createBuffer(sizeof(SceneUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "RenderingUniforms");

    initAtomColors();
    initAtomQuadBuffer();
    initBoxBuffer();
    initBondBuffer();
    initFieldArrowBuffer();
    initGridLineBuffer();
    initPotentialFieldQuadBuffer();
    initMemoryOrderBuffer();
}

void RendererWGPU::initSharedPipelines() {
    initBoxPipeline(lineWGSL);
    initBondPipeline(lineWGSL);
    initMemoryOrderPipeline(memlineWGSL);
    initGridPipeline(gridWGSL);
    initPotentialFieldPipeline(fieldWGSL);
    initFieldArrowPipeline(arrowsWGSL);
    initFieldContourPipeline(contourWGSL);
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

void RendererWGPU::drawWorldPass(wgpu::TextureView targetView, wgpu::TextureView depthView, const RenderData& renderData, wgpu::LoadOp targetLoadOp, bool applySelection) {
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
    for (size_t i = 0; i < atomLayer_.typeColorsData.size(); ++i) {
        uniforms.typeColors[i] = atomLayer_.typeColorsData[i];
    }
    currentSceneUniforms_ = uniforms;
    lineUniformSlotIndex_ = 0;
    gridUniformSlotIndex_ = 0;

    WGPUContext::instance().queue()->writeBuffer(*uniformBuffer, 0, &uniforms, sizeof(uniforms));

    beginPass(targetView, depthView, targetLoadOp);

    if (renderData.drawAtoms) {
        drawAtomsImpl(renderData.atoms, renderData, applySelection);
    }
    if (renderData.drawBonds && !renderData.bonds.empty()) {
        setLineColor(glm::vec4(0.4f, 0.6f, 1.0f, 0.3f));
        drawBondsImpl(renderData.atoms, renderData.bonds);
    }
    if (renderData.drawGrid && !renderData.grid.empty()) {
        drawGridImpl(renderData.grid);
    }
    if (renderData.drawVectorField && !renderData.vectorField.empty()) {
        drawVectorFieldImpl(renderData);
    }
    if (renderData.drawFieldContours && !renderData.vectorField.empty()) {
        drawFieldContoursImpl(renderData);
    }
    if (renderData.drawFieldArrows && !renderData.vectorField.empty()) {
        drawFieldArrowsImpl(renderData);
    }
    if (renderData.drawBox && renderData.hasBox) {
        setLineColor(applySelection ? glm::vec4(0.5f, 0.78f, 1.0f, 0.55f) : glm::vec4(0.35f, 0.52f, 0.9f, 0.3f));
        drawBoxImpl(renderData.worldSize);
    }
    if (renderData.drawMemoryOrder) {
        setLineColor(glm::vec4(0.35f, 0.52f, 0.9f, 0.3f));
        drawMemoryOrderImpl(renderData.atoms);
    }
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
