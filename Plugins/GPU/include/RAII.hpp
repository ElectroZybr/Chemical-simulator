#pragma once

#include <webgpu/webgpu.h>
#include <span>
#include <utility>
#include <cassert>

namespace GPU {

template <typename Handle, void (*ReleaseFn)(Handle), void (*DestroyFn)(Handle) = nullptr>
class HandleWrapper {
public:
    HandleWrapper() noexcept = default;
    explicit HandleWrapper(Handle h) noexcept : handle_(h) {}

    ~HandleWrapper() { release(); }

    // move-only
    HandleWrapper(HandleWrapper&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    HandleWrapper& operator=(HandleWrapper&& other) noexcept {
        if (this != &other) {
            release();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    HandleWrapper(const HandleWrapper&) = delete;
    HandleWrapper& operator=(const HandleWrapper&) = delete;

    Handle get() const noexcept { return handle_; }
    Handle release_ownership() noexcept { return std::exchange(handle_, nullptr); }

    explicit operator bool() const noexcept { return handle_ != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return handle_ == nullptr; }
    bool operator!=(std::nullptr_t) const noexcept { return handle_ != nullptr; }

    Handle operator->() const noexcept { return handle_; }

protected:
    void release() noexcept {
        if (handle_) {
            if constexpr (DestroyFn != nullptr) {
                DestroyFn(handle_);
            }
            ReleaseFn(handle_);
            handle_ = nullptr;
        }
    }

    Handle handle_ = nullptr;
};

// ============================================================
// Конкретные типы
// ============================================================

// ----- Buffer -----
class Buffer : public HandleWrapper<WGPUBuffer, wgpuBufferRelease, wgpuBufferDestroy> {
public:
    using HandleWrapper::HandleWrapper;

    void destroy() {
        if (handle_) {
            wgpuBufferDestroy(handle_);
        }
    }

    void* getMappedRange(size_t offset, size_t size) {
        return wgpuBufferGetMappedRange(handle_, offset, size);
    }

    const void* getConstMappedRange(size_t offset, size_t size) const {
        return wgpuBufferGetConstMappedRange(handle_, offset, size);
    }

    void unmap() {
        wgpuBufferUnmap(handle_);
    }
};

// ----- Texture -----
class Texture : public HandleWrapper<WGPUTexture, wgpuTextureRelease, wgpuTextureDestroy> {
public:
    using HandleWrapper::HandleWrapper;

    void destroy() {
        if (handle_) {
            wgpuTextureDestroy(handle_);
        }
    }

    WGPUTextureFormat getFormat() const {
        return wgpuTextureGetFormat(handle_);
    }

    uint32_t getWidth() const  { return wgpuTextureGetWidth(handle_); }
    uint32_t getHeight() const { return wgpuTextureGetHeight(handle_); }
    uint32_t getDepthOrArrayLayers() const {
        return wgpuTextureGetDepthOrArrayLayers(handle_);
    }
};

// ----- TextureView -----
class TextureView : public HandleWrapper<WGPUTextureView, wgpuTextureViewRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- Sampler -----
class Sampler : public HandleWrapper<WGPUSampler, wgpuSamplerRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- BindGroupLayout -----
class BindGroupLayout : public HandleWrapper<WGPUBindGroupLayout, wgpuBindGroupLayoutRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- BindGroup -----
class BindGroup : public HandleWrapper<WGPUBindGroup, wgpuBindGroupRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- ShaderModule -----
class ShaderModule : public HandleWrapper<WGPUShaderModule, wgpuShaderModuleRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- PipelineLayout -----
class PipelineLayout : public HandleWrapper<WGPUPipelineLayout, wgpuPipelineLayoutRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- RenderPipeline -----
class RenderPipeline : public HandleWrapper<WGPURenderPipeline, wgpuRenderPipelineRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- ComputePipeline -----
class ComputePipeline : public HandleWrapper<WGPUComputePipeline, wgpuComputePipelineRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- CommandEncoder -----
class CommandEncoder : public HandleWrapper<WGPUCommandEncoder, wgpuCommandEncoderRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- CommandBuffer -----
class CommandBuffer : public HandleWrapper<WGPUCommandBuffer, wgpuCommandBufferRelease> {
public:
    using HandleWrapper::HandleWrapper;
};

// ----- QuerySet -----
class QuerySet : public HandleWrapper<WGPUQuerySet, wgpuQuerySetRelease, wgpuQuerySetDestroy> {
public:
    using HandleWrapper::HandleWrapper;
};

// ============================================================
// Device / Queue / Surface
// ============================================================

class Queue : public HandleWrapper<WGPUQueue, wgpuQueueRelease> {
public:
    using HandleWrapper::HandleWrapper;

    void submit(std::span<const WGPUCommandBuffer> commands) {
        wgpuQueueSubmit(handle_, static_cast<uint32_t>(commands.size()), commands.data());
    }

    void submit(WGPUCommandBuffer command) {
        wgpuQueueSubmit(handle_, 1, &command);
    }

    void writeBuffer(WGPUBuffer buffer, uint64_t bufferOffset,
                     const void* data, size_t size) {
        wgpuQueueWriteBuffer(handle_, buffer, bufferOffset, data, size);
    }

    void writeTexture(const WGPUTexelCopyTextureInfo* destination,
                      const void* data, size_t dataSize,
                      const WGPUTexelCopyBufferLayout* dataLayout,
                      const WGPUExtent3D* writeSize) {
        wgpuQueueWriteTexture(handle_, destination, data, dataSize, dataLayout, writeSize);
    }
};

class Device : public HandleWrapper<WGPUDevice, wgpuDeviceRelease> {
public:
    using HandleWrapper::HandleWrapper;

    Queue getQueue() const {
        return Queue(wgpuDeviceGetQueue(handle_));
    }

    // удобные методы создания (по желанию)
    Buffer createBuffer(const WGPUBufferDescriptor& desc) {
        return Buffer(wgpuDeviceCreateBuffer(handle_, &desc));
    }

    Texture createTexture(const WGPUTextureDescriptor& desc) {
        return Texture(wgpuDeviceCreateTexture(handle_, &desc));
    }

    Sampler createSampler(const WGPUSamplerDescriptor& desc) {
        return Sampler(wgpuDeviceCreateSampler(handle_, &desc));
    }

    BindGroupLayout createBindGroupLayout(const WGPUBindGroupLayoutDescriptor& desc) {
        return BindGroupLayout(wgpuDeviceCreateBindGroupLayout(handle_, &desc));
    }

    BindGroup createBindGroup(const WGPUBindGroupDescriptor& desc) {
        return BindGroup(wgpuDeviceCreateBindGroup(handle_, &desc));
    }

    ShaderModule createShaderModule(const WGPUShaderModuleDescriptor& desc) {
        return ShaderModule(wgpuDeviceCreateShaderModule(handle_, &desc));
    }

    PipelineLayout createPipelineLayout(const WGPUPipelineLayoutDescriptor& desc) {
        return PipelineLayout(wgpuDeviceCreatePipelineLayout(handle_, &desc));
    }

    RenderPipeline createRenderPipeline(const WGPURenderPipelineDescriptor& desc) {
        return RenderPipeline(wgpuDeviceCreateRenderPipeline(handle_, &desc));
    }

    ComputePipeline createComputePipeline(const WGPUComputePipelineDescriptor& desc) {
        return ComputePipeline(wgpuDeviceCreateComputePipeline(handle_, &desc));
    }

    CommandEncoder createCommandEncoder(const WGPUCommandEncoderDescriptor* desc = nullptr) {
        return CommandEncoder(wgpuDeviceCreateCommandEncoder(handle_, desc));
    }

    void poll(bool wait = false, WGPUSubmissionIndex const* submissionIndex = nullptr) {
        wgpuDevicePoll(handle_, wait, submissionIndex);
    }
};

class Surface : public HandleWrapper<WGPUSurface, wgpuSurfaceRelease> {
public:
    using HandleWrapper::HandleWrapper;

    void configure(const WGPUSurfaceConfiguration& config) {
        wgpuSurfaceConfigure(handle_, &config);
    }

    void unconfigure() {
        wgpuSurfaceUnconfigure(handle_);
    }

    void present() {
        wgpuSurfacePresent(handle_);
    }

    WGPUTextureFormat getPreferredFormat(WGPUAdapter adapter) const {
        WGPUSurfaceCapabilities caps = {};
        wgpuSurfaceGetCapabilities(handle_, adapter, &caps);

        WGPUTextureFormat format = caps.formats[0];
        for (uint32_t i = 0; i < caps.formatCount; ++i) {
            if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm ||
                caps.formats[i] == WGPUTextureFormat_RGBA8Unorm) {
                format = caps.formats[i];
                break;
            }
        }

        wgpuSurfaceCapabilitiesFreeMembers(caps);
        return format;
    }

    // Получить текущую текстуру (для рендера)
    // Внимание: TextureView нужно релизить после использования!
    WGPUSurfaceTexture getCurrentTexture() {
        WGPUSurfaceTexture surfaceTexture = {};
        wgpuSurfaceGetCurrentTexture(handle_, &surfaceTexture);
        return surfaceTexture;
    }
};
} // namespace GPU