#pragma once

#include <string_view>
#include <span>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include "NativeWindow.hpp"

namespace GPU {

class WGPU {
public:
    static WGPU& instance() {
        static WGPU ctx;
        return ctx;
    }

    void init(const NativeWindow& window, uint32_t width, uint32_t height);
    void initHeadless(uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);

    WGPUBuffer createBuffer(size_t bytes, WGPUBufferUsage usage,
                            std::string_view label, bool mappedAtCreation = false);

    WGPUBindGroupLayout createBindGroupLayout(std::span<const WGPUBindGroupLayoutEntry> entries,
                                              std::string_view label);

    WGPUBindGroup createBindGroup(WGPUBindGroupLayout layout,
                                  std::span<const WGPUBindGroupEntry> entries,
                                  std::string_view label);

    WGPUDevice        device()        const { return device_; }
    WGPUQueue         queue()         const { return queue_; }
    WGPUSurface       surface()       const { return surface_; }
    WGPUTextureFormat surfaceFormat() const { return surfaceFormat_; }
    WGPUTextureView   depthView()     const { return depthTextureView_; }

    uint32_t width()  const { return width_; }
    uint32_t height() const { return height_; }

    void present() { wgpuSurfacePresent(surface_); }
    void processEvents() { wgpuDevicePoll(device_, false, nullptr); }
    void waitIdle() { wgpuDevicePoll(device_, true, nullptr); }

    ~WGPU() { shutdown(); }
    void shutdown();

private:
    WGPUPresentMode choosePresentMode(const WGPUSurfaceCapabilities& caps);
    WGPUSurface createSurface(const NativeWindow& window);
    void createDepthTexture(uint32_t width, uint32_t height);
    void createInstance();
    void createDevice();
    void configureSurface(uint32_t width, uint32_t height);

    bool initialized_       = false;
    bool surfaceConfigured_ = false;

    WGPUInstance     instance_          = nullptr;
    WGPUAdapter      adapter_           = nullptr;
    WGPUDevice       device_            = nullptr;
    WGPUQueue        queue_             = nullptr;
    WGPUSurface      surface_           = nullptr;
    WGPUTexture      depthTexture_      = nullptr;
    WGPUTextureView  depthTextureView_  = nullptr;

    WGPUTextureFormat surfaceFormat_ = WGPUTextureFormat_Undefined;
    uint32_t width_  = 0;
    uint32_t height_ = 0;
};

} // namespace GPU