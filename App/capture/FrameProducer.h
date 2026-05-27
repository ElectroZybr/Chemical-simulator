#pragma once

#include "BufferPool.h"
#include "CaptureSettings.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <webgpu/webgpu.hpp>

struct ImageData {
    std::vector<std::byte> pixels;
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    uint32_t width = 0;
    uint32_t height = 0;
};

class FFmpegStreamer;

class FrameProducer {
public:
    void startVideoCapture(FFmpegStreamer* streamer, const CaptureSettings& settings);
    void stopVideoCapture();

    using ScreenshotCallback = std::function<void(ImageData)>;
    void requestScreenshot(ScreenshotCallback callback);

    wgpu::TextureView acquireRenderTarget(wgpu::Texture surfaceTexture, wgpu::TextureView surfaceView);
    void onFrameRendered(wgpu::Texture texture, float renderDeltaTime);
    void blitToSurface(wgpu::Texture surfaceTexture);

    uint64_t capturedFrameCount() const { return capturedFrameCount_; }

private:
    struct MapContext {
        wgpu::Buffer buffer;
        BufferPool* pool;
        FFmpegStreamer* streamer; // nullptr для скриншота
        uint32_t width;
        uint32_t height;
        uint32_t bytesPerRow;    // WebGPU-aligned stride (кратно 256)
        const char* inputPixFmt; // статическая строка "rgba"/"bgra"
        wgpu::TextureFormat format;
        ScreenshotCallback onScreenshot; // пустой если это видеокадр
    };

    static void onBufferMapped(WGPUMapAsyncStatus status, WGPUStringView view, void* userdata1, void* userdata2);

    static std::vector<std::byte> stripPadding(const std::byte* src, uint32_t width, uint32_t height, uint32_t bytesPerRow);

    // bytesPerRow должен быть кратен 256 (wgpu::COPY_BYTES_PER_ROW_ALIGNMENT)
    static uint32_t calcBytesPerRow(uint32_t width) {
        constexpr uint32_t kAlign = 256;
        return (width * kBytesPerPixel + kAlign - 1) & ~(kAlign - 1);
    }

    static constexpr uint32_t kBytesPerPixel = 4;
    static constexpr size_t kPoolBufferCount = 3;

    BufferPool pool;

    FFmpegStreamer* streamer = nullptr;
    bool videoActive = false;
    double frameAccum_ = 0.0;
    uint32_t targetFps_ = 30;
    uint64_t capturedFrameCount_ = 0;

    ScreenshotCallback pendingScreenshot_;

    void ensureIntermediateTexture(uint32_t width, uint32_t height, wgpu::TextureFormat format);

    wgpu::raii::Texture intermediate_;
    wgpu::raii::TextureView intermediateView_;
};
