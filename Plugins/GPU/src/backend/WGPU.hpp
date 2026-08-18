#pragma once

#include <span>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

// Source
#include "GPUAPI.hpp"

namespace GPU {

class WGPU final : public GPUAPI {
public:
    static WGPU& instance() {
        static WGPU ctx;
        return ctx;
    }

    void init(GLFWwindow* window, uint32_t width, uint32_t height) override;
    void initHeadless(uint32_t width, uint32_t height) override;
    void resize(uint32_t width, uint32_t height) override;

    wgpu::Buffer createBuffer(size_t bytes, wgpu::BufferUsage usage, std::string_view label, bool mappedAtCreation = false);
    wgpu::BindGroupLayout createBindGroupLayout(std::span<const wgpu::BindGroupLayoutEntry> entries, std::string_view label);
    wgpu::BindGroup createBindGroup(wgpu::BindGroupLayout layout, std::span<const wgpu::BindGroupEntry> entries, std::string_view label);

    const wgpu::raii::Device& device() const { return device_; }
    const wgpu::raii::Queue& queue() const { return queue_; }
    const wgpu::raii::Surface& surface() const { return surface_; }
    wgpu::TextureFormat surfaceFormat() const { return surfaceFormat_; }
    const wgpu::raii::TextureView& depthView() const { return depthTextureView_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }

    void present() { surface_->present(); }
    void processEvents() override { device_->poll(false, nullptr); }
    void waitIdle() override { device_->poll(true, nullptr); }

    ~WGPU() { shutdown(); }
    void shutdown() override;

private:
    wgpu::PresentMode choosePresentMode(const wgpu::SurfaceCapabilities& caps);
    wgpu::Surface createSurface(GLFWwindow* window);
    void createDepthTexture(uint32_t width, uint32_t height);
    void createInstance();
    void createDevice();
    void configureSurface(uint32_t width, uint32_t height);

    bool initialized_ = false;
    bool surfaceConfigured_ = false;

    wgpu::raii::Instance instance_;
    wgpu::raii::Adapter adapter_;
    wgpu::raii::Device device_;
    wgpu::raii::Queue queue_;
    wgpu::raii::Surface surface_;
    wgpu::raii::Texture depthTexture_;
    wgpu::raii::TextureView depthTextureView_;

    wgpu::TextureFormat surfaceFormat_ = wgpu::TextureFormat::Undefined;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};
}