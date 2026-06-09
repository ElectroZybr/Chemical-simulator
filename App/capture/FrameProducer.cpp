#include "FrameProducer.h"

#include "FfmpegStreamer.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>

#include <webgpu.h>
#include <webgpu/webgpu-raii.hpp>

#include "Lattice/Engine/metrics/Profiler.h"
#include "Rendering/backend/WGPUContext.h"

void FrameProducer::startVideoCapture(FFmpegStreamer* streamer, const CaptureSettings& settings) {
    this->streamer = streamer;
    targetFps_ = settings.fps;
    frameAccum_ = 0.0;
    capturedFrameCount_ = 0;
    videoActive = true;
}

void FrameProducer::stopVideoCapture() {
    videoActive = false;
    streamer = nullptr;
    capturedFrameCount_ = 0;
}

void FrameProducer::requestScreenshot(ScreenshotCallback callback) { pendingScreenshot_ = std::move(callback); }

wgpu::TextureView FrameProducer::acquireRenderTarget(wgpu::Texture surfaceTexture, wgpu::TextureView surfaceView) {
    if (!videoActive && !pendingScreenshot_) {
        return surfaceView;
    }

    ensureIntermediateTexture(surfaceTexture.getWidth(), surfaceTexture.getHeight(), surfaceTexture.getFormat());
    return *intermediateView_;
}

void FrameProducer::onFrameRendered(wgpu::Texture surfaceTexture, float renderDeltaTime) {
    PROFILE_SCOPE("FrameProducer::onFrameRendered");

    bool wantVideo = false;
    if (videoActive) {
        frameAccum_ += renderDeltaTime;
        if (frameAccum_ >= 1.0 / targetFps_) {
            frameAccum_ -= 1.0 / targetFps_;
            wantVideo = true;
        }
    }

    const bool wantScreenshot = static_cast<bool>(pendingScreenshot_);
    const bool captureActive = videoActive || wantScreenshot;

    if (!wantVideo && !wantScreenshot) {
        if (captureActive && intermediate_) {
            blitToSurface(surfaceTexture);
        }
        return;
    }

    const uint32_t width = intermediate_->getWidth();
    const uint32_t height = intermediate_->getHeight();
    const wgpu::TextureFormat format = intermediate_->getFormat();
    const uint32_t bytesPerRow = calcBytesPerRow(width);
    const size_t bufferSize = static_cast<size_t>(height) * bytesPerRow;

    if (!pool.isInitialized()) {
        pool.init(bufferSize, kPoolBufferCount);
    }

    const char* pixFmt = capture_utils::toInputPixelFormat(format);

    auto submitCapture = [&](FFmpegStreamer* s, ScreenshotCallback callback) {
        wgpu::Buffer buffer = pool.acquire();

        if (!buffer) {
            return;
        }

        wgpu::TexelCopyTextureInfo src{};
        src.texture = *intermediate_;
        src.mipLevel = 0;
        src.origin = {0, 0, 0};
        src.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferInfo dst{};
        dst.buffer = buffer;
        dst.layout.offset = 0;
        dst.layout.bytesPerRow = bytesPerRow;
        dst.layout.rowsPerImage = height;

        wgpu::Extent3D extent{width, height, 1};
        wgpu::raii::CommandEncoder encoder = WGPUContext::instance().device()->createCommandEncoder();
        encoder->copyTextureToBuffer(src, dst, extent);
        wgpu::raii::CommandBuffer cmds = encoder->finish();
        WGPUContext::instance().queue()->submit(1, &*cmds);

        std::unique_ptr<MapContext> ctx = std::make_unique<MapContext>();
        ctx->buffer = buffer;
        ctx->pool = &pool;
        ctx->streamer = s;
        ctx->width = width;
        ctx->height = height;
        ctx->bytesPerRow = bytesPerRow;
        ctx->inputPixFmt = pixFmt;
        ctx->format = format;
        ctx->onScreenshot = std::move(callback);

        wgpu::BufferMapCallbackInfo callbackInfo{};
        callbackInfo.mode = wgpu::CallbackMode::AllowSpontaneous;
        callbackInfo.callback = FrameProducer::onBufferMapped;
        callbackInfo.userdata1 = ctx.release();
        callbackInfo.userdata2 = nullptr;

        buffer.mapAsync(wgpu::MapMode::Read, 0, bufferSize, callbackInfo);

        if (s != nullptr) {
            ++capturedFrameCount_;
        }
    };

    if (wantScreenshot) {
        submitCapture(nullptr, std::move(pendingScreenshot_));
        pendingScreenshot_ = nullptr;
    }

    if (wantVideo) {
        submitCapture(streamer, {});
    }

    blitToSurface(surfaceTexture);
}

void FrameProducer::blitToSurface(wgpu::Texture surfaceTexture) {
    const uint32_t width = intermediate_->getWidth();
    const uint32_t height = intermediate_->getHeight();

    wgpu::TexelCopyTextureInfo src{};
    src.texture = *intermediate_;
    src.mipLevel = 0;
    src.origin = {0, 0, 0};
    src.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = surfaceTexture;
    dst.mipLevel = 0;
    dst.origin = {0, 0, 0};
    dst.aspect = wgpu::TextureAspect::All;

    wgpu::Extent3D extent{width, height, 1};
    wgpu::raii::CommandEncoder enc = WGPUContext::instance().device()->createCommandEncoder();
    enc->copyTextureToTexture(src, dst, extent);
    wgpu::raii::CommandBuffer cmds = enc->finish();
    WGPUContext::instance().queue()->submit(1, &*cmds);
}

void FrameProducer::onBufferMapped(WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
    PROFILE_SCOPE("FrameProducer::onBufferMapped");

    std::unique_ptr<MapContext> ctx(static_cast<MapContext*>(userdata1));

    if (status != WGPUMapAsyncStatus_Success) {
        ctx->pool->release(std::move(ctx->buffer));
        return;
    }

    const size_t mappedSize = static_cast<size_t>(ctx->height) * ctx->bytesPerRow;
    const std::byte* raw = static_cast<const std::byte*>(ctx->buffer.getConstMappedRange(0, mappedSize));

    std::vector<std::byte> pixels = stripPadding(raw, ctx->width, ctx->height, ctx->bytesPerRow);

    ctx->buffer.unmap();
    ctx->pool->release(std::move(ctx->buffer));

    if (ctx->onScreenshot) {
        ImageData img;
        img.width = ctx->width;
        img.height = ctx->height;
        img.format = ctx->format;
        img.pixels.resize(pixels.size());
        std::ranges::copy(pixels, img.pixels.begin());

        ctx->onScreenshot(std::move(img));
    }
    else if (ctx->streamer) {
        bool pushed = ctx->streamer->pushFrame(std::move(pixels));
        (void)pushed;
    }
}

std::vector<std::byte> FrameProducer::stripPadding(const std::byte* src, uint32_t width, uint32_t height, uint32_t bytesPerRow) {
    PROFILE_SCOPE("FrameProducer::stripPadding");

    const uint32_t rowBytes = width * kBytesPerPixel;
    std::vector<std::byte> out(static_cast<size_t>(height) * rowBytes);

    for (uint32_t row = 0; row < height; ++row) {
        std::memcpy(out.data() + row * rowBytes, src + row * bytesPerRow, rowBytes);
    }
    return out;
}

void FrameProducer::ensureIntermediateTexture(uint32_t width, uint32_t height, wgpu::TextureFormat format) {
    if (intermediate_ && intermediate_->getWidth() == width && intermediate_->getHeight() == height) {
        return;
    }

    wgpu::TextureDescriptor desc{};
    desc.size = {width, height, 1};
    desc.format = format;
    desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = wgpu::TextureDimension::_2D;
    intermediate_ = WGPUContext::instance().device()->createTexture(desc);
    intermediateView_ = intermediate_->createView();

    pool = BufferPool{};
}
