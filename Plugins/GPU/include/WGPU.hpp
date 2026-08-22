#pragma once

#include <string_view>
#include <span>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include "NativeWindow.hpp"

namespace GPU {

class WGPU {
public:
    void init();
    void initHeadless();
    WGPUSurface createSurface(const NativeWindow&);
    WGPUTextureFormat configureSurface(WGPUSurface, uint32_t w, uint32_t h);
    WGPUTexture createDepthTexture(uint32_t w, uint32_t h);
    WGPUTextureView createDepthTextureView(WGPUTexture);
    WGPUBuffer createBuffer(size_t bytes, WGPUBufferUsage usage, std::string_view label, bool mappedAtCreation = false);
    WGPUBindGroupLayout createBindGroupLayout(std::span<const WGPUBindGroupLayoutEntry> entries, std::string_view label);
    WGPUBindGroup createBindGroup(WGPUBindGroupLayout layout,
                                  std::span<const WGPUBindGroupEntry> entries,
                                  std::string_view label);
    WGPUPresentMode choosePresentMode(const WGPUSurfaceCapabilities& caps);

    WGPUDevice device() const { return device_; }
    WGPUQueue queue() const { return queue_; }

    void processEvents() { wgpuDevicePoll(device_, false, nullptr); }
    void waitIdle() { wgpuDevicePoll(device_, true, nullptr); }

    ~WGPU() { shutdown(); }
    void shutdown();

private:
    void createInstance();
    void createDevice();

    bool initialized_       = false;
    bool surfaceConfigured_ = false;

    WGPUInstance instance_ = nullptr;
    WGPUAdapter  adapter_  = nullptr;
    WGPUDevice   device_   = nullptr;
    WGPUQueue    queue_    = nullptr;
};

} // namespace GPU