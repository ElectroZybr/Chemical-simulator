#include "WGPU.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

namespace GPU {

// ---------- helpers ----------
static WGPUStringView toWGPUString(std::string_view s) {
    return WGPUStringView{s.data(), static_cast<size_t>(s.size())};
}

// ---------- init ----------
void WGPU::init(const NativeWindow& window, uint32_t width, uint32_t height) {
    if (initialized_) return;

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

// ---------- initHeadless ----------
void WGPU::initHeadless(uint32_t width, uint32_t height) {
    if (initialized_) return;

    createInstance();
    createDevice();

    surfaceFormat_ = WGPUTextureFormat_RGBA8Unorm;
    createDepthTexture(width, height);

    width_ = width;
    height_ = height;
    initialized_ = true;
}

// ---------- resize ----------
void WGPU::resize(uint32_t width, uint32_t height) {
    if (!initialized_ || (width == width_ && height == height_))
        return;

    configureSurface(width, height);
    createDepthTexture(width, height);

    width_ = width;
    height_ = height;
}

// ---------- createBuffer ----------
WGPUBuffer WGPU::createBuffer(size_t bytes, WGPUBufferUsage usage,
                              std::string_view label, bool mappedAtCreation)
{
    WGPUBufferDescriptor desc = {};
    desc.nextInChain = nullptr;
    desc.label = toWGPUString(label);
    desc.usage = usage;
    desc.size = bytes;
    desc.mappedAtCreation = mappedAtCreation;

    return wgpuDeviceCreateBuffer(device_, &desc);
}

// ---------- createBindGroupLayout ----------
WGPUBindGroupLayout WGPU::createBindGroupLayout(
    std::span<const WGPUBindGroupLayoutEntry> entries,
    std::string_view label)
{
    WGPUBindGroupLayoutDescriptor desc = {};
    desc.nextInChain = nullptr;
    desc.label = toWGPUString(label);
    desc.entryCount = static_cast<uint32_t>(entries.size());
    desc.entries = entries.data();

    return wgpuDeviceCreateBindGroupLayout(device_, &desc);
}

// ---------- createBindGroup ----------
WGPUBindGroup WGPU::createBindGroup(WGPUBindGroupLayout layout,
                                    std::span<const WGPUBindGroupEntry> entries,
                                    std::string_view label)
{
    WGPUBindGroupDescriptor desc = {};
    desc.nextInChain = nullptr;
    desc.label = toWGPUString(label);
    desc.layout = layout;
    desc.entryCount = static_cast<uint32_t>(entries.size());
    desc.entries = entries.data();

    return wgpuDeviceCreateBindGroup(device_, &desc);
}

// ---------- choosePresentMode ----------
WGPUPresentMode WGPU::choosePresentMode(const WGPUSurfaceCapabilities& caps) {
    auto supports = [&](WGPUPresentMode mode) {
        for (uint32_t i = 0; i < caps.presentModeCount; ++i) {
            if (caps.presentModes[i] == mode)
                return true;
        }
        return false;
    };

    if (supports(WGPUPresentMode_Mailbox))     return WGPUPresentMode_Mailbox;
    if (supports(WGPUPresentMode_FifoRelaxed)) return WGPUPresentMode_FifoRelaxed;
    if (supports(WGPUPresentMode_Immediate))   return WGPUPresentMode_Immediate;
    return WGPUPresentMode_Fifo;
}

// ---------- createSurface ----------
WGPUSurface WGPU::createSurface(const NativeWindow& window) {
    WGPUSurfaceDescriptor desc = {};
    desc.label = toWGPUString("Surface");

    switch (window.kind) {
#if defined(__linux__)
    case NativeWindow::Kind::X11: {
        WGPUSurfaceSourceXlibWindow xlibSrc = {};
        xlibSrc.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
        xlibSrc.display = window.display;
        xlibSrc.window  = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window.window));

        desc.nextInChain = &xlibSrc.chain;
        return wgpuInstanceCreateSurface(instance_, &desc);
    }
    case NativeWindow::Kind::Wayland: {
        WGPUSurfaceSourceWaylandSurface src = {};
        src.chain.next  = nullptr;
        src.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
        src.display = window.display;
        src.surface = window.window;

        WGPUSurfaceDescriptor desc = {};
        desc.label = toWGPUString("Surface");
        desc.nextInChain = &src.chain;

        return wgpuInstanceCreateSurface(instance_, &desc);
    }
#endif

#if defined(_WIN32)
    case NativeWindow::Kind::Win32: {
        WGPUSurfaceSourceWindowsHWND src = {};
        src.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        src.hinstance = window.extra
            ? window.extra
            : GetModuleHandle(nullptr);
        src.hwnd = static_cast<HWND>(window.window);

        desc.nextInChain = &src.chain;
        return wgpuInstanceCreateSurface(instance_, &desc);
    }
#endif

#if defined(__APPLE__)
    case NativeWindow::Kind::Metal: {
        WGPUSurfaceSourceMetalLayer src = {};
        src.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
        src.layer = window.extra;
        desc.nextInChain = &src.chain;
        return wgpuInstanceCreateSurface(instance_, &desc);
    }
#endif

    case NativeWindow::Kind::Headless:
    case NativeWindow::Kind::None:
    default:
        return nullptr;
    }

    return nullptr;
}

// ---------- createDepthTexture ----------
void WGPU::createDepthTexture(uint32_t width, uint32_t height) {
    if (depthTextureView_) {
        wgpuTextureViewRelease(depthTextureView_);
        depthTextureView_ = nullptr;
    }
    if (depthTexture_) {
        wgpuTextureDestroy(depthTexture_);
        wgpuTextureRelease(depthTexture_);
        depthTexture_ = nullptr;
    }

    WGPUTextureDescriptor depthDesc = {};
    depthDesc.label = toWGPUString("Depth Texture");
    depthDesc.dimension = WGPUTextureDimension_2D;
    depthDesc.size = {width, height, 1};
    depthDesc.format = WGPUTextureFormat_Depth24Plus;
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    depthDesc.usage = WGPUTextureUsage_RenderAttachment;

    depthTexture_ = wgpuDeviceCreateTexture(device_, &depthDesc);

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.label = toWGPUString("Depth Texture View");
    viewDesc.format = WGPUTextureFormat_Depth24Plus;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_DepthOnly;

    depthTextureView_ = wgpuTextureCreateView(depthTexture_, &viewDesc);
}

// ---------- createInstance ----------
void WGPU::createInstance() {
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;

#ifndef NDEBUG
    WGPUInstanceExtras extras = {};
    extras.chain.next = nullptr;
    extras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
    extras.flags = WGPUInstanceFlag_Debug | WGPUInstanceFlag_Validation;
    desc.nextInChain = &extras.chain;
#endif

    instance_ = wgpuCreateInstance(&desc);
    if (!instance_)
        throw std::runtime_error("wgpu: failed to create instance");
}

// ---------- createDevice ----------
void WGPU::createDevice() {
    // --- Request Adapter ---
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
    if (surface_)
        adapterOpts.compatibleSurface = surface_;

    struct AdapterUserData {
        WGPUAdapter adapter = nullptr;
        bool done = false;
    } adapterData;

    WGPURequestAdapterCallbackInfo adapterCb = {};
    adapterCb.mode = WGPUCallbackMode_AllowSpontaneous;
    adapterCb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                            WGPUStringView message, void* userdata1, void*) {
        auto* data = static_cast<AdapterUserData*>(userdata1);
        if (status == WGPURequestAdapterStatus_Success) {
            data->adapter = adapter;
        } else {
            std::cerr << "wgpu requestAdapter failed: "
                      << std::string_view(message.data, message.length) << '\n';
        }
        data->done = true;
    };
    adapterCb.userdata1 = &adapterData;

    wgpuInstanceRequestAdapter(instance_, &adapterOpts, adapterCb);

    while (!adapterData.done) {
        wgpuInstanceProcessEvents(instance_);
    }

    adapter_ = adapterData.adapter;
    if (!adapter_)
        throw std::runtime_error("wgpu: failed to get adapter");

    // --- Request Device ---
    WGPUDeviceDescriptor deviceDesc = {};

    // device lost callback
    deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceDesc.deviceLostCallbackInfo.callback =
        [](WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message,
           void*, void*) {
            std::cerr << "wgpu device lost (" << static_cast<int>(reason) << "): "
                      << std::string_view(message.data, message.length) << '\n';
        };

    // uncaptured error callback
    deviceDesc.uncapturedErrorCallbackInfo.callback =
        [](WGPUDevice const*, WGPUErrorType type, WGPUStringView message,
           void*, void*) {
            std::cerr << "wgpu error (" << static_cast<int>(type) << "): "
                      << std::string_view(message.data, message.length) << '\n';
        };

    struct DeviceUserData {
        WGPUDevice device = nullptr;
        bool done = false;
    } deviceData;

    WGPURequestDeviceCallbackInfo deviceCb = {};
    deviceCb.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceCb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                           WGPUStringView message, void* userdata1, void*) {
        auto* data = static_cast<DeviceUserData*>(userdata1);
        if (status == WGPURequestDeviceStatus_Success) {
            data->device = device;
        } else {
            std::cerr << "wgpu requestDevice failed: "
                      << std::string_view(message.data, message.length) << '\n';
        }
        data->done = true;
    };
    deviceCb.userdata1 = &deviceData;

    wgpuAdapterRequestDevice(adapter_, &deviceDesc, deviceCb);

    while (!deviceData.done) {
        wgpuInstanceProcessEvents(instance_);
    }

    device_ = deviceData.device;
    if (!device_)
        throw std::runtime_error("wgpu: failed to get device");

    queue_ = wgpuDeviceGetQueue(device_);
}

// ---------- configureSurface ----------
void WGPU::configureSurface(uint32_t width, uint32_t height) {
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(surface_, adapter_, &caps);

    surfaceFormat_ = caps.formats[0];
    for (uint32_t i = 0; i < caps.formatCount; ++i) {
        if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm ||
            caps.formats[i] == WGPUTextureFormat_RGBA8Unorm) {
            surfaceFormat_ = caps.formats[i];
            break;
        }
    }

    WGPUSurfaceConfiguration config = {};
    config.device = device_;
    config.format = surfaceFormat_;
    config.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
    config.width = width;
    config.height = height;
    config.presentMode = choosePresentMode(caps);
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(surface_, &config);
    surfaceConfigured_ = true;

    // caps нужно освободить
    wgpuSurfaceCapabilitiesFreeMembers(caps);
}

// ---------- shutdown ----------
void WGPU::shutdown() {
    if (!initialized_) return;

    if (depthTextureView_) {
        wgpuTextureViewRelease(depthTextureView_);
        depthTextureView_ = nullptr;
    }
    if (depthTexture_) {
        wgpuTextureDestroy(depthTexture_);
        wgpuTextureRelease(depthTexture_);
        depthTexture_ = nullptr;
    }

    if (surface_ && surfaceConfigured_) {
        wgpuSurfaceUnconfigure(surface_);
    }
    surfaceConfigured_ = false;

    if (queue_) {
        wgpuQueueRelease(queue_);
        queue_ = nullptr;
    }
    if (device_) {
        wgpuDeviceRelease(device_);
        device_ = nullptr;
    }
    if (surface_) {
        wgpuSurfaceRelease(surface_);
        surface_ = nullptr;
    }
    if (adapter_) {
        wgpuAdapterRelease(adapter_);
        adapter_ = nullptr;
    }
    if (instance_) {
        wgpuInstanceRelease(instance_);
        instance_ = nullptr;
    }

    initialized_ = false;
}

} // namespace GPU