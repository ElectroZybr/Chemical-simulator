#include "WGPU.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace GPU {

static WGPUStringView toWGPUString(std::string_view s) {
    return WGPUStringView{s.data(), static_cast<size_t>(s.size())};
}

// ---------- init ----------
void WGPU::init() {
    if (initialized_) return;
    createInstance();
    createDevice();
    initialized_ = true;
}

void WGPU::initHeadless() {
    init();
}

// ---------- resources ----------
WGPUBuffer WGPU::createBuffer(size_t bytes, WGPUBufferUsage usage,
                              std::string_view label, bool mappedAtCreation)
{
    WGPUBufferDescriptor desc = {};
    desc.label = toWGPUString(label);
    desc.usage = usage;
    desc.size = bytes;
    desc.mappedAtCreation = mappedAtCreation;
    return wgpuDeviceCreateBuffer(device_, &desc);
}

WGPUBindGroupLayout WGPU::createBindGroupLayout(
    std::span<const WGPUBindGroupLayoutEntry> entries,
    std::string_view label)
{
    WGPUBindGroupLayoutDescriptor desc = {};
    desc.label = toWGPUString(label);
    desc.entryCount = static_cast<uint32_t>(entries.size());
    desc.entries = entries.data();
    return wgpuDeviceCreateBindGroupLayout(device_, &desc);
}

WGPUBindGroup WGPU::createBindGroup(WGPUBindGroupLayout layout,
                                    std::span<const WGPUBindGroupEntry> entries,
                                    std::string_view label)
{
    WGPUBindGroupDescriptor desc = {};
    desc.label = toWGPUString(label);
    desc.layout = layout;
    desc.entryCount = static_cast<uint32_t>(entries.size());
    desc.entries = entries.data();
    return wgpuDeviceCreateBindGroup(device_, &desc);
}

WGPUPresentMode WGPU::choosePresentMode(const WGPUSurfaceCapabilities& caps) {
    auto supports = [&](WGPUPresentMode mode) {
        for (uint32_t i = 0; i < caps.presentModeCount; ++i)
            if (caps.presentModes[i] == mode)
                return true;
        return false;
    };

    if (supports(WGPUPresentMode_Mailbox))     return WGPUPresentMode_Mailbox;
    if (supports(WGPUPresentMode_FifoRelaxed)) return WGPUPresentMode_FifoRelaxed;
    if (supports(WGPUPresentMode_Immediate))   return WGPUPresentMode_Immediate;
    return WGPUPresentMode_Fifo;
}

// ---------- surface (фабрика, без хранения в GPU) ----------
WGPUSurface WGPU::createSurface(const NativeWindow& window) {
    if (!instance_)
        throw std::runtime_error("wgpu: createSurface before init");

    WGPUSurfaceDescriptor desc = {};
    desc.label = toWGPUString("Surface");

    switch (window.kind) {
#if defined(__linux__)
    case NativeWindow::Kind::X11: {
        WGPUSurfaceSourceXlibWindow src = {};
        src.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
        src.display = window.display;
        src.window = static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(window.window));
        desc.nextInChain = &src.chain;
        return wgpuInstanceCreateSurface(instance_, &desc);
    }
    case NativeWindow::Kind::Wayland: {
        WGPUSurfaceSourceWaylandSurface src = {};
        src.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
        src.display = window.display;
        src.surface = window.window;
        desc.nextInChain = &src.chain;
        return wgpuInstanceCreateSurface(instance_, &desc);
    }
#endif
#if defined(_WIN32)
    case NativeWindow::Kind::Win32: {
        WGPUSurfaceSourceWindowsHWND src = {};
        src.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        src.hinstance = window.extra ? window.extra : GetModuleHandle(nullptr);
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
}

WGPUTextureFormat WGPU::configureSurface(WGPUSurface surface, uint32_t width, uint32_t height) {
    if (!surface || !device_ || !adapter_)
        throw std::runtime_error("wgpu: configureSurface invalid state");
    if (width == 0 || height == 0)
        throw std::runtime_error("wgpu: configureSurface zero size");

    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(surface, adapter_, &caps);

    WGPUTextureFormat format = caps.formats[0];
    for (uint32_t i = 0; i < caps.formatCount; ++i) {
        if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm ||
            caps.formats[i] == WGPUTextureFormat_RGBA8Unorm) {
            format = caps.formats[i];
            break;
        }
    }

    WGPUSurfaceConfiguration config = {};
    config.device = device_;
    config.format = format;
    config.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
    config.width = width;
    config.height = height;
    config.presentMode = choosePresentMode(caps);
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(surface, &config);
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    return format;
}

WGPUTexture WGPU::createDepthTexture(uint32_t width, uint32_t height) {
    WGPUTextureDescriptor depthDesc = {};
    depthDesc.label = toWGPUString("Depth Texture");
    depthDesc.dimension = WGPUTextureDimension_2D;
    depthDesc.size = {width, height, 1};
    depthDesc.format = WGPUTextureFormat_Depth24Plus;
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    depthDesc.usage = WGPUTextureUsage_RenderAttachment;
    return wgpuDeviceCreateTexture(device_, &depthDesc);
}

WGPUTextureView WGPU::createDepthTextureView(WGPUTexture depthTexture) {
    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.label = toWGPUString("Depth Texture View");
    viewDesc.format = WGPUTextureFormat_Depth24Plus;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_DepthOnly;
    return wgpuTextureCreateView(depthTexture, &viewDesc);
}

// ---------- instance / device ----------
void WGPU::createInstance() {
    WGPUInstanceDescriptor desc = {};

#ifndef NDEBUG
    WGPUInstanceExtras extras = {};
    extras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
    extras.flags = WGPUInstanceFlag_Debug | WGPUInstanceFlag_Validation;
    desc.nextInChain = &extras.chain;
#endif

    instance_ = wgpuCreateInstance(&desc);
    if (!instance_)
        throw std::runtime_error("wgpu: failed to create instance");
}

void WGPU::createDevice() {
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;

    struct AdapterUserData {
        WGPUAdapter adapter = nullptr;
        bool done = false;
    } adapterData;

    WGPURequestAdapterCallbackInfo adapterCb = {};
    adapterCb.mode = WGPUCallbackMode_AllowSpontaneous;
    adapterCb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                            WGPUStringView message, void* userdata1, void*) {
        auto* data = static_cast<AdapterUserData*>(userdata1);
        if (status == WGPURequestAdapterStatus_Success)
            data->adapter = adapter;
        else
            std::cerr << "wgpu requestAdapter failed: "
                      << std::string_view(message.data, message.length) << '\n';
        data->done = true;
    };
    adapterCb.userdata1 = &adapterData;

    wgpuInstanceRequestAdapter(instance_, &adapterOpts, adapterCb);
    while (!adapterData.done)
        wgpuInstanceProcessEvents(instance_);

    adapter_ = adapterData.adapter;
    if (!adapter_)
        throw std::runtime_error("wgpu: failed to get adapter");

    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceDesc.deviceLostCallbackInfo.callback =
        [](WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message,
           void*, void*) {
            std::cerr << "wgpu device lost (" << static_cast<int>(reason) << "): "
                      << std::string_view(message.data, message.length) << '\n';
        };
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
        if (status == WGPURequestDeviceStatus_Success)
            data->device = device;
        else
            std::cerr << "wgpu requestDevice failed: "
                      << std::string_view(message.data, message.length) << '\n';
        data->done = true;
    };
    deviceCb.userdata1 = &deviceData;

    wgpuAdapterRequestDevice(adapter_, &deviceDesc, deviceCb);
    while (!deviceData.done)
        wgpuInstanceProcessEvents(instance_);

    device_ = deviceData.device;
    if (!device_)
        throw std::runtime_error("wgpu: failed to get device");

    queue_ = wgpuDeviceGetQueue(device_);
}

// ---------- shutdown ----------
void WGPU::shutdown() {
    if (!initialized_) return;
    if (queue_)    { wgpuQueueRelease(queue_);       queue_ = nullptr; }
    if (device_)   { wgpuDeviceRelease(device_);     device_ = nullptr; }
    if (adapter_)  { wgpuAdapterRelease(adapter_);   adapter_ = nullptr; }
    if (instance_) { wgpuInstanceRelease(instance_); instance_ = nullptr; }

    initialized_ = false;
}

} // namespace GPU