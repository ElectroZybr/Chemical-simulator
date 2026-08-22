#include "Render.hpp"

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
#include "Shell/include/WindowAPI.hpp"

// Plugin dependences
#include "WGPU.hpp"
#include "WindowAPI.hpp"


struct Render::FrameState {
    WGPUSurface surface = nullptr;
    WGPUTextureFormat format = WGPUTextureFormat_Undefined;
    WGPUTexture depth = nullptr;
    WGPUTextureView depthView = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    bool surfaceConfigured = false;
};

Render::Render(Lattice::Components& renderer)
    : frameState(std::make_unique<FrameState>()) {
    settings = renderer.require<Lattice::Settings>();
    gpu_ = renderer.add<GPU::WGPU>();
    window_ = renderer.get<WindowAPI>();
    Logger::info("Render", "render created");
}

void Render::setup() {
    auto& window = *window_;

    ensureSurface(*window_);
    const auto fb = window_->framebufferSize();
    resize(uint32_t(fb.x), uint32_t(fb.y));
}

Render::~Render() {
    releaseFrameResources();
    Logger::info("Render", "destroying object");
}

void Render::releaseFrameResources() {
    if (!frameState) return;

    if (frameState->depthView) {
        wgpuTextureViewRelease(frameState->depthView);
        frameState->depthView = nullptr;
    }
    if (frameState->depth) {
        wgpuTextureDestroy(frameState->depth);
        wgpuTextureRelease(frameState->depth);
        frameState->depth = nullptr;
    }
    if (frameState->surface) {
        if (frameState->surfaceConfigured) {
            wgpuSurfaceUnconfigure(frameState->surface);
            frameState->surfaceConfigured = false;
        }
        wgpuSurfaceRelease(frameState->surface);
        frameState->surface = nullptr;
    }

    frameState->width = 0;
    frameState->height = 0;
}

void Render::ensureSurface(WindowAPI& window) {
    if (frameState->surface) return;
    gpu_->init();
    const auto n = window.native();
    frameState->surface = gpu_->createSurface(window.native());
    if (!frameState->surface)
        throw std::runtime_error("Render: failed to create surface");
}

void Render::resize(uint32_t w, uint32_t h) {
    if (!frameState->surface || w == 0 || h == 0) return;
    if (w == frameState->width && h == frameState->height && frameState->surfaceConfigured)
        return;

    // depth
    if (frameState->depthView) {
        wgpuTextureViewRelease(frameState->depthView);
        frameState->depthView = nullptr;
    }
    if (frameState->depth) {
        wgpuTextureDestroy(frameState->depth);
        wgpuTextureRelease(frameState->depth);
        frameState->depth = nullptr;
    }

    frameState->format = gpu_->configureSurface(frameState->surface, w, h);
    frameState->surfaceConfigured = true;

    frameState->depth = gpu_->createDepthTexture(w, h);
    frameState->depthView = gpu_->createDepthTextureView(frameState->depth);

    frameState->width = w;
    frameState->height = h;
}

void Render::frame() {
    if (!frameState->surface || !frameState->surfaceConfigured) return;

    WindowAPI& window = *window_;
    if (window.shouldClose()) return;
    const auto fb = window.framebufferSize();
    const uint32_t w = uint32_t(fb.x);
    const uint32_t h = uint32_t(fb.y);
    if (w == 0 || h == 0) return;

    resize(w, h);

    WGPUSurfaceTexture st{};
    wgpuSurfaceGetCurrentTexture(frameState->surface, &st);

    const bool ok =
        st.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal ||
        st.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;

    if (!ok) {
        if (st.texture)
            wgpuTextureRelease(st.texture);

        // Outdated / Lost / Error → часто нужен reconfigure
        if (st.status == WGPUSurfaceGetCurrentTextureStatus_Outdated ||
            st.status == WGPUSurfaceGetCurrentTextureStatus_Lost) {
            frameState->surfaceConfigured = false;
            resize(w, h);
        }
        return;
    }

    WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);

    WGPUCommandEncoderDescriptor encDesc{};
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(gpu_->device(), &encDesc);

    WGPURenderPassColorAttachment color{};
    color.view = view;
    color.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED; // если есть в твоём header
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = {0.1, 0.2, 0.3, 1.0};

    WGPURenderPassDepthStencilAttachment depthAtt{};
    depthAtt.view = frameState->depthView;
    depthAtt.depthLoadOp = WGPULoadOp_Clear;
    depthAtt.depthStoreOp = WGPUStoreOp_Store;
    depthAtt.depthClearValue = 1.0f;
    depthAtt.depthReadOnly = false;
    depthAtt.stencilLoadOp = WGPULoadOp_Undefined;
    depthAtt.stencilStoreOp = WGPUStoreOp_Undefined;
    depthAtt.stencilReadOnly = true;

    WGPURenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &color;
    passDesc.depthStencilAttachment = frameState->depthView ? &depthAtt : nullptr;

    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(gpu_->queue(), 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    wgpuSurfacePresent(frameState->surface);

    wgpuTextureViewRelease(view);
    wgpuTextureRelease(st.texture);
}