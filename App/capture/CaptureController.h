#pragma once

#include "CaptureSettings.h"
#include "FfmpegStreamer.h"
#include "FrameProducer.h"

#include <cstdint>
#include <filesystem>
#include <functional>

#include <webgpu/webgpu-raii.hpp>

struct UiState;

class CaptureController {
public:
    CaptureController();

    [[nodiscard]] CaptureSettings settings() const noexcept { return settings_; }
    [[nodiscard]] std::filesystem::path outputDirectory() const { return outputDirectory_; }
    void setSettings(const CaptureSettings& settings) noexcept;
    void setOutputDirectory(const std::filesystem::path& path);

    void start();
    void stop();
    void toggle();
    void handleToggleShortcut();

    using ScreenshotCallback = std::function<void(ImageData)>;
    void requestScreenshot(ScreenshotCallback callback);

    wgpu::TextureView acquireRenderTarget(wgpu::Texture surfaceTexture);
    void onFrameRendered(wgpu::Texture texture);

    void update(double deltaTime);

    void syncUiState(UiState& uiState) const;

    [[nodiscard]] bool isAvailable() const noexcept { return available_; }
    [[nodiscard]] bool isRecording() const noexcept { return streamer_.isRunning(); }
    [[nodiscard]] uint64_t savedFrameCount() const noexcept { return producer_.capturedFrameCount(); }
    [[nodiscard]] float captureFps() const noexcept { return captureFps_; }
    [[nodiscard]] double blinkElapsed() const noexcept { return blinkElapsed_; }

private:
    [[nodiscard]] std::filesystem::path makeCaptureOutputPath() const;
    void resetSessionStats();

    bool available_ = false;
    CaptureSettings settings_{};
    std::filesystem::path outputDirectory_ = "captures";

    FrameProducer producer_;
    FFmpegStreamer streamer_;

    uint64_t lastFrameCountSample_ = 0;
    double captureRateAccum_ = 0.0;
    float captureFps_ = 0.0f;
    double blinkElapsed_ = 0.0;

    uint32_t measuredRenderFps_ = 60;
    double renderFpsAccum_ = 0.0;
    uint32_t renderFrameCount_ = 0;
    std::chrono::steady_clock::time_point lastRenderTime_;

    bool toggleShortcutHeld_ = false;

    wgpu::TextureFormat activeFormat_ = wgpu::TextureFormat::Undefined;
    uint32_t activeWidth_ = 0;
    uint32_t activeHeight_ = 0;
};
