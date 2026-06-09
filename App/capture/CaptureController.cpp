#include "CaptureController.h"
#include "CaptureOutputPath.h"

#include <chrono>
#include <cctype>
#include <iostream>
#include <string>

#include "Lattice/Engine/metrics/Profiler.h"
#include "GUI/interface/UiState.h"
#include "GUI/io/keyboard/Keyboard.h"
#include "Rendering/BaseRenderer.h"
#include "Rendering/backend/WGPUContext.h"

CaptureController::CaptureController() {

#ifdef _WIN32
    available_ = (std::system("ffmpeg -version >NUL 2>&1") == 0);
#else
    available_ = (std::system("ffmpeg -version >/dev/null 2>&1") == 0);
#endif
}

void CaptureController::setSettings(const CaptureSettings& settings) noexcept { settings_ = settings; }

namespace {
#ifndef _WIN32
    bool looksLikeForeignPath(const std::filesystem::path& path) {
        const std::string value = path.string();
        return value.find('\\') != std::string::npos ||
               (value.size() >= 2 && std::isalpha(static_cast<unsigned char>(value[0])) && value[1] == ':');
    }
#endif
}

void CaptureController::setOutputDirectory(const std::filesystem::path& path) {
#ifndef _WIN32
    if (path.empty() || looksLikeForeignPath(path)) {
        outputDirectory_ = "captures";
        return;
    }
#else
    if (path.empty()) {
        outputDirectory_ = "captures";
        return;
    }
#endif
    outputDirectory_ = path;
}

void CaptureController::start() {
    if (!available_ || isRecording()) {
        return;
    }

    lastRenderTime_ = std::chrono::steady_clock::now();
    renderFrameCount_ = 0;
    renderFpsAccum_ = 0.0;
    measuredRenderFps_ = 0;

    std::error_code fsError;
    std::filesystem::create_directories(outputDirectory_, fsError);
    if (fsError) {
        std::cerr << "Failed to create capture directory '" << outputDirectory_.string() << "': " << fsError.message() << "\n";
        return;
    }
    const std::filesystem::path outputPath = makeCaptureOutputPath();
    const char* pixFmt = capture_utils::toInputPixelFormat(activeFormat_);

    if (!streamer_.start(activeWidth_, activeHeight_, pixFmt, settings_, outputPath)) {
        return;
    }

    producer_.startVideoCapture(&streamer_, settings_);
    resetSessionStats();
}

void CaptureController::stop() {
    if (!isRecording()) {
        return;
    }

    producer_.stopVideoCapture();
    streamer_.stop();
}

void CaptureController::toggle() { isRecording() ? stop() : start(); }

void CaptureController::handleToggleShortcut() {
    const bool captureKeyPressed = Keyboard::isPressed(GLFW_KEY_F8);
    if (isAvailable() && captureKeyPressed && !toggleShortcutHeld_) {
        toggle();
    }
    toggleShortcutHeld_ = captureKeyPressed;
}

void CaptureController::requestScreenshot(ScreenshotCallback callback) { producer_.requestScreenshot(std::move(callback)); }

wgpu::TextureView CaptureController::acquireRenderTarget(wgpu::Texture surfaceTexture, wgpu::TextureView surfaceView) {
    return producer_.acquireRenderTarget(surfaceTexture, surfaceView);
}

void CaptureController::onFrameRendered(wgpu::Texture texture) {
    PROFILE_SCOPE("CaptureController::onFrameRendered");

    activeFormat_ = texture.getFormat();
    activeWidth_ = texture.getWidth();
    activeHeight_ = texture.getHeight();

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> diff = now - lastRenderTime_;
    lastRenderTime_ = now;

    producer_.onFrameRendered(texture, diff.count());
    ++renderFrameCount_;
}

void CaptureController::renderFrame(BaseRenderer& renderer, const std::function<void()>& afterDraw) {
    WGPUContext& ctx = WGPUContext::instance();

    wgpu::SurfaceTexture surfaceTex;
    ctx.surface()->getCurrentTexture(&surfaceTex);
    wgpu::raii::Texture surfaceTexture(surfaceTex.texture);
    wgpu::raii::TextureView surfaceView = surfaceTexture->createView();

    const wgpu::TextureView renderTarget = acquireRenderTarget(*surfaceTexture, *surfaceView);
    renderer.drawShot(renderTarget, *ctx.depthView());

    if (afterDraw) {
        afterDraw();
    }

    renderer.endFrame();
    onFrameRendered(*surfaceTexture);
    ctx.present();
    ctx.processEvents();
}

void CaptureController::update(double deltaTime) {
    renderFpsAccum_ += deltaTime;
    if (renderFpsAccum_ >= 1.0) {
        measuredRenderFps_ = static_cast<uint32_t>(renderFrameCount_ / renderFpsAccum_);
        renderFrameCount_ = 0;
        renderFpsAccum_ = 0.0;
    }

    constexpr double kFpsSampleInterval = 1.0;
    captureRateAccum_ += deltaTime;
    if (captureRateAccum_ >= kFpsSampleInterval) {
        const uint64_t current = savedFrameCount();
        const uint64_t framesThisInterval = current - lastFrameCountSample_;
        captureFps_ = static_cast<float>(framesThisInterval / captureRateAccum_);
        lastFrameCountSample_ = current;
        captureRateAccum_ = 0.0;
    }

    if (isRecording()) {
        blinkElapsed_ += deltaTime;
    }
    else {
        blinkElapsed_ = 0.0;
    }
}

void CaptureController::syncUiState(UiState& uiState) const {
    uiState.captureAvailable = available_;
    uiState.captureRecording = isRecording();
    uiState.captureFps = captureFps_;
    uiState.captureBlinkElapsed = blinkElapsed_;
    uiState.captureFrameCount = producer_.capturedFrameCount();
}

std::filesystem::path CaptureController::makeCaptureOutputPath() const {
    return capture_utils::makeDatedCaptureOutputPath(outputDirectory_, ".mp4");
}

void CaptureController::resetSessionStats() {
    lastFrameCountSample_ = producer_.capturedFrameCount();
    captureRateAccum_ = 0.0;
    captureFps_ = 0.0f;
    blinkElapsed_ = 0.0;
}
