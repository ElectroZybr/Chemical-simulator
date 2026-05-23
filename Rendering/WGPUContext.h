#pragma once

#include <iostream>
#include <stdexcept>

#include <webgpu/webgpu.hpp>
#include <wgpu.h>

#if defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#elif defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#include "MetalBackend.h"
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

class WGPUContext {
public:
    static WGPUContext& instance() {
        static WGPUContext ctx;
        return ctx;
    }

    void init(GLFWwindow* window, uint32_t width, uint32_t height) {
        if (initialized_) {
            return;
        }

        wgpu::InstanceDescriptor instanceDesc{};

#ifndef NDEBUG
        WGPUInstanceExtras extras{};
        extras.chain.sType = (WGPUSType)WGPUNativeSType::WGPUSType_InstanceExtras;
        extras.flags = WGPUInstanceFlag_Debug;
        instanceDesc.nextInChain = &extras.chain;
#endif

        instance_ = wgpu::createInstance(instanceDesc);
        if (!instance_) {
            throw std::runtime_error("wgpu: failed to create instance");
        }

        surface_ = createSurface(window);
        if (!surface_) {
            throw std::runtime_error("wgpu: failed to create surface");
        }

        wgpu::RequestAdapterOptions adapterOpts = wgpu::Default;
        adapterOpts.compatibleSurface = surface_;
        adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;
        adapter_ = instance_.requestAdapter(adapterOpts);
        if (!adapter_) {
            throw std::runtime_error("wgpu: failed to get adapter");
        }

        wgpu::DeviceDescriptor deviceDesc = wgpu::Default;
        deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView msg, void*, void*) {
            std::cerr << "wgpu device lost (" << reason << "): " << std::string_view(msg.data, msg.length) << "\n";
        };
        deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const*, WGPUErrorType type, WGPUStringView msg, void*, void*) {
            std::cerr << "wgpu error (" << type << "): " << std::string_view(msg.data, msg.length) << "\n";
        };

        device_ = adapter_.requestDevice(deviceDesc);
        if (!device_) {
            throw std::runtime_error("wgpu: failed to get device");
        }

        queue_ = device_.getQueue();

        wgpu::SurfaceCapabilities caps{};
        surface_.getCapabilities(adapter_, &caps);
        surfaceFormat_ = caps.formats[0];
        for (size_t i = 0; i < caps.formatCount; ++i) {
            if (caps.formats[i] == wgpu::TextureFormat::BGRA8Unorm || caps.formats[i] == wgpu::TextureFormat::RGBA8Unorm) {
                surfaceFormat_ = caps.formats[i];
                break;
            }
        }

        wgpu::SurfaceConfiguration surfaceConfig{};
        surfaceConfig.device = device_;
        surfaceConfig.format = surfaceFormat_;
        surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst;
        surfaceConfig.width = width;
        surfaceConfig.height = height;
        surfaceConfig.presentMode = choosePresentMode(caps);
        surface_.configure(surfaceConfig);

        createDepthTexture(width, height);

        width_ = width;
        height_ = height;
        initialized_ = true;
    }

    void initHeadless(uint32_t width, uint32_t height) {
        if (initialized_) {
            return;
        }

        wgpu::InstanceDescriptor instanceDesc{};
        instance_ = wgpu::createInstance(instanceDesc);
        if (!instance_) {
            throw std::runtime_error("wgpu: failed to create instance");
        }

        // Без surface — просто берём любой адаптер
        wgpu::RequestAdapterOptions adapterOpts{};
        adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;
        adapter_ = instance_.requestAdapter(adapterOpts);
        if (!adapter_) {
            throw std::runtime_error("wgpu: failed to get adapter");
        }

        wgpu::DeviceDescriptor deviceDesc{};
        deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView msg, void*, void*) {
            std::cerr << "wgpu device lost (" << reason << "): " << std::string_view(msg.data, msg.length) << "\n";
        };
        deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const*, WGPUErrorType type, WGPUStringView msg, void*, void*) {
            std::cerr << "wgpu error (" << type << "): " << std::string_view(msg.data, msg.length) << "\n";
        };
        device_ = adapter_.requestDevice(deviceDesc);
        if (!device_) {
            throw std::runtime_error("wgpu: failed to get device");
        }

        queue_ = device_.getQueue();
        surfaceFormat_ = wgpu::TextureFormat::RGBA8Unorm; // фиксированный формат для headless

        createDepthTexture(width, height);
        width_ = width;
        height_ = height;
        initialized_ = true;
    }

    void resize(uint32_t width, uint32_t height) {
        if (!initialized_ || (width == width_ && height == height_)) {
            return;
        }

        wgpu::SurfaceCapabilities caps{};
        surface_.getCapabilities(adapter_, &caps);

        wgpu::SurfaceConfiguration surfaceConfig{};
        surfaceConfig.device = device_;
        surfaceConfig.format = surfaceFormat_;
        surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst;
        surfaceConfig.width = width;
        surfaceConfig.height = height;
        surfaceConfig.presentMode = choosePresentMode(caps);
        surface_.configure(surfaceConfig);

        createDepthTexture(width, height);

        width_ = width;
        height_ = height;
    }

    void present() { surface_.present(); }
    void processEvents() { device_.poll(false, nullptr); }

    void shutdown() {
        if (!initialized_) {
            return;
        }
        depthTextureView_ = nullptr;
        depthTexture_ = nullptr;
        surface_.unconfigure();
        queue_ = nullptr;
        device_ = nullptr;
        adapter_ = nullptr;
        surface_ = nullptr;
        instance_ = nullptr;
        initialized_ = false;
    }

    ~WGPUContext() { shutdown(); }

    wgpu::Device device() const { return device_; }
    wgpu::Queue queue() const { return queue_; }
    wgpu::Surface surface() const { return surface_; }
    wgpu::TextureFormat surfaceFormat() const { return surfaceFormat_; }
    wgpu::TextureView depthView() const { return depthTextureView_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }

private:
    WGPUContext() = default;

    static wgpu::PresentMode choosePresentMode(const wgpu::SurfaceCapabilities& caps) {
        auto supports = [&](wgpu::PresentMode mode) {
            for (size_t i = 0; i < caps.presentModeCount; ++i) {
                if (caps.presentModes[i] == mode) {
                    return true;
                }
            }
            return false;
        };

        if (supports(wgpu::PresentMode::Mailbox)) {
            return wgpu::PresentMode::Mailbox;
        }
        if (supports(wgpu::PresentMode::FifoRelaxed)) {
            return wgpu::PresentMode::FifoRelaxed;
        }
        if (supports(wgpu::PresentMode::Immediate)) {
            return wgpu::PresentMode::Immediate;
        }
        return wgpu::PresentMode::Fifo;
    }

    wgpu::Surface createSurface(GLFWwindow* window) {
#if defined(__linux__)
        wgpu::SurfaceSourceXlibWindow xlibSrc = wgpu::Default;
        xlibSrc.display = glfwGetX11Display();
        xlibSrc.window = glfwGetX11Window(window);

        wgpu::SurfaceDescriptor desc{};
        desc.label = wgpu::StringView("Surface");
        desc.nextInChain = &xlibSrc.chain;
        return instance_.createSurface(desc);

#elif defined(_WIN32)
        wgpu::SurfaceSourceWindowsHWND hwndSrc = wgpu::Default;
        hwndSrc.hinstance = GetModuleHandle(nullptr);
        hwndSrc.hwnd = glfwGetWin32Window(window);

        wgpu::SurfaceDescriptor desc{};
        desc.label = wgpu::StringView("Surface");
        desc.nextInChain = &hwndSrc.chain;
        return instance_.createSurface(desc);

#elif defined(__APPLE__)
        wgpu::SurfaceSourceMetalLayer metalSrc = wgpu::Default;
        metalSrc.layer = getMetalBackend(window);
        wgpu::SurfaceDescriptor desc{};
        desc.label = wgpu::StringView("Surface");
        desc.nextInChain = &metalSrc.chain;
        return instance_.createSurface(desc);
#endif
    }

    void createDepthTexture(uint32_t width, uint32_t height) {
        depthTextureView_ = nullptr;
        depthTexture_ = nullptr;

        wgpu::TextureDescriptor depthDesc{};
        depthDesc.label = wgpu::StringView("Depth Texture");
        depthDesc.size = {width, height, 1};
        depthDesc.format = wgpu::TextureFormat::Depth24Plus;
        depthDesc.usage = wgpu::TextureUsage::RenderAttachment;
        depthDesc.mipLevelCount = 1;
        depthDesc.sampleCount = 1;
        depthDesc.dimension = wgpu::TextureDimension::_2D;
        depthTexture_ = device_.createTexture(depthDesc);

        wgpu::TextureViewDescriptor viewDesc{};
        viewDesc.label = wgpu::StringView("Depth Texture View");
        viewDesc.format = wgpu::TextureFormat::Depth24Plus;
        viewDesc.dimension = wgpu::TextureViewDimension::_2D;
        viewDesc.mipLevelCount = 1;
        viewDesc.arrayLayerCount = 1;
        viewDesc.aspect = wgpu::TextureAspect::DepthOnly;
        depthTextureView_ = depthTexture_.createView(viewDesc);
    }

    bool initialized_ = false;

    wgpu::Instance instance_ = nullptr;
    wgpu::Adapter adapter_ = nullptr;
    wgpu::Device device_ = nullptr;
    wgpu::Queue queue_ = nullptr;
    wgpu::Surface surface_ = nullptr;
    wgpu::Texture depthTexture_ = nullptr;
    wgpu::TextureView depthTextureView_ = nullptr;

    wgpu::TextureFormat surfaceFormat_ = wgpu::TextureFormat::Undefined;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};
