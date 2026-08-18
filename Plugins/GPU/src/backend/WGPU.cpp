#include "WGPU.hpp"

#if defined(__linux__)
    #define GLFW_EXPOSE_NATIVE_X11
#elif defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace GPU {
    void WGPU::init(GLFWwindow* window, uint32_t width, uint32_t height) {
        if (initialized_) {
            return;
        }

        createInstance();
        surface_ = createSurface(window);
        if (!surface_)
            throw std::runtime_error("wgpu: failed to create surface");

        createDevice();
        configureSurface(width, height);
        createDepthTexture(width, height);

        width_ = width;
        height_ = height;
        initialized_ = true;
    }

    void WGPU::initHeadless(uint32_t width, uint32_t height) {
        if (initialized_) {
            return;
        }

        createInstance();
        createDevice();
        surfaceFormat_ = wgpu::TextureFormat::RGBA8Unorm;
        createDepthTexture(width, height);

        width_ = width;
        height_ = height;
        initialized_ = true;
    }

    void WGPU::resize(uint32_t width, uint32_t height) {
        if (!initialized_ ||
            (width == width_ && height == height_)) {
            return;
        }

        configureSurface(width, height);
        createDepthTexture(width, height);

        width_ = width;
        height_ = height;
    }

    wgpu::Buffer WGPU::createBuffer(size_t bytes, wgpu::BufferUsage usage, std::string_view label, bool mappedAtCreation) {
        wgpu::BufferDescriptor desc{};
        desc.label = wgpu::StringView(label);
        desc.size = bytes;
        desc.usage = usage;
        desc.mappedAtCreation = mappedAtCreation;
        return device_->createBuffer(desc);
    }

    wgpu::BindGroupLayout WGPU::createBindGroupLayout(std::span<const wgpu::BindGroupLayoutEntry> entries, std::string_view label) {
        wgpu::BindGroupLayoutDescriptor bglDesc{};
        bglDesc.entryCount = entries.size();
        bglDesc.entries = entries.data();
        bglDesc.label = wgpu::StringView(label);
        return device_->createBindGroupLayout(bglDesc);
    }

    wgpu::BindGroup WGPU::createBindGroup(wgpu::BindGroupLayout layout, std::span<const wgpu::BindGroupEntry> entries, std::string_view label) {
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = layout;
        bgDesc.entryCount = entries.size();
        bgDesc.entries = entries.data();
        bgDesc.label = wgpu::StringView(label);
        return device_->createBindGroup(bgDesc);
    }

    wgpu::PresentMode WGPU::choosePresentMode(const wgpu::SurfaceCapabilities& caps) {
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

    wgpu::Surface WGPU::createSurface(GLFWwindow* window) {
    #if defined(__linux__)
        wgpu::SurfaceSourceXlibWindow xlibSrc = wgpu::Default;
        xlibSrc.display = glfwGetX11Display();
        xlibSrc.window = glfwGetX11Window(window);

        wgpu::SurfaceDescriptor desc{};
        desc.label = wgpu::StringView("Surface");
        desc.nextInChain = &xlibSrc.chain;
        return instance_->createSurface(desc);

    #elif defined(_WIN32)
        wgpu::SurfaceSourceWindowsHWND hwndSrc = wgpu::Default;
        hwndSrc.hinstance = GetModuleHandle(nullptr);
        hwndSrc.hwnd = glfwGetWin32Window(window);

        wgpu::SurfaceDescriptor desc{};
        desc.label = wgpu::StringView("Surface");
        desc.nextInChain = &hwndSrc.chain;
        return instance_->createSurface(desc);

    #elif defined(__APPLE__)
        wgpu::SurfaceSourceMetalLayer metalSrc = wgpu::Default;
        metalSrc.layer = getMetalBackend(window);
        wgpu::SurfaceDescriptor desc{};
        desc.label = wgpu::StringView("Surface");
        desc.nextInChain = &metalSrc.chain;
        return instance_->createSurface(desc);
    #endif
    }

    void WGPU::createDepthTexture(uint32_t width, uint32_t height) {
        wgpu::TextureDescriptor depthDesc{};
        depthDesc.label = wgpu::StringView("Depth Texture");
        depthDesc.size = {width, height, 1};
        depthDesc.format = wgpu::TextureFormat::Depth24Plus;
        depthDesc.usage = wgpu::TextureUsage::RenderAttachment;
        depthDesc.mipLevelCount = 1;
        depthDesc.sampleCount = 1;
        depthDesc.dimension = wgpu::TextureDimension::_2D;
        depthTexture_ = device_->createTexture(depthDesc);

        wgpu::TextureViewDescriptor viewDesc{};
        viewDesc.label = wgpu::StringView("Depth Texture View");
        viewDesc.format = wgpu::TextureFormat::Depth24Plus;
        viewDesc.dimension = wgpu::TextureViewDimension::_2D;
        viewDesc.mipLevelCount = 1;
        viewDesc.arrayLayerCount = 1;
        viewDesc.aspect = wgpu::TextureAspect::DepthOnly;
        depthTextureView_ = depthTexture_->createView(viewDesc);
    }

    void WGPU::createInstance() {
        wgpu::InstanceDescriptor instanceDesc{};

        #ifndef NDEBUG
            WGPUInstanceExtras extras{};
            extras.chain.sType =
                (WGPUSType)WGPUNativeSType::WGPUSType_InstanceExtras;
            extras.flags =
                WGPUInstanceFlag_Debug |
                WGPUInstanceFlag_Validation;
            instanceDesc.nextInChain = &extras.chain;
        #endif

        instance_ = wgpu::createInstance(instanceDesc);

        if (!instance_) {
            throw std::runtime_error("wgpu: failed to create instance");
        }
    }

    void WGPU::createDevice() {
        wgpu::RequestAdapterOptions adapterOpts = wgpu::Default;
        adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;

        if (surface_) {
            adapterOpts.compatibleSurface = *surface_;
        }

        adapter_ = instance_->requestAdapter(adapterOpts);
        if (!adapter_) {
            throw std::runtime_error("wgpu: failed to get adapter");
        }

        wgpu::DeviceDescriptor deviceDesc = wgpu::Default;
        deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView msg, void*, void*) {
                std::cerr << "wgpu device lost (" << reason << "): " << std::string_view(msg.data, msg.length) << '\n';
            };

        deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const*, WGPUErrorType type, WGPUStringView msg, void*, void*) {
                std::cerr << "wgpu error (" << type << "): " << std::string_view(msg.data, msg.length) << '\n';
            };

        device_ = adapter_->requestDevice(deviceDesc);
        if (!device_) {
            throw std::runtime_error("wgpu: failed to get device");
        }

        queue_ = device_->getQueue();
    }

    void WGPU::configureSurface(uint32_t width, uint32_t height) {
        wgpu::SurfaceCapabilities caps{};
        surface_->getCapabilities(*adapter_, &caps);

        surfaceFormat_ = caps.formats[0];

        for (size_t i = 0; i < caps.formatCount; ++i) {
            if (caps.formats[i] == wgpu::TextureFormat::BGRA8Unorm ||
                caps.formats[i] == wgpu::TextureFormat::RGBA8Unorm) {
                surfaceFormat_ = caps.formats[i];
                break;
            }
        }

        wgpu::SurfaceConfiguration config{};
        config.device = *device_;
        config.format = surfaceFormat_;
        config.usage =
            wgpu::TextureUsage::RenderAttachment |
            wgpu::TextureUsage::CopyDst;
        config.width = width;
        config.height = height;
        config.presentMode = choosePresentMode(caps);

        surface_->configure(config);
        surfaceConfigured_ = true;
    }

    void WGPU::shutdown() {
        if (!initialized_) {
            return;
        }
        depthTextureView_ = wgpu::raii::TextureView{};
        depthTexture_ = wgpu::raii::Texture{};
        if (surface_ && surfaceConfigured_) {
            surface_->unconfigure();
        }
        surfaceConfigured_ = false;
        queue_ = wgpu::raii::Queue{};
        device_ = wgpu::raii::Device{};
        surface_ = wgpu::raii::Surface{};
        adapter_ = wgpu::raii::Adapter{};
        instance_ = wgpu::raii::Instance{};
        initialized_ = false;
    }
}