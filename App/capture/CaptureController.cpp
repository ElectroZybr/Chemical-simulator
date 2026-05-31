#include "CaptureController.h"

#include <cassert>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <format>
#include <iostream>
#include <optional>
#include <string>

#include "Engine/metrics/Profiler.h"
#include "GUI/interface/UiState.h"
#include "GUI/io/keyboard/Keyboard.h"

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

    std::optional<uint32_t> parseDailyCaptureIndex(const std::filesystem::path& path, std::string_view datePrefix) {
        if (path.extension() != ".mp4") {
            return std::nullopt;
        }

        const std::string stem = path.stem().string();
        const std::string prefix = std::string(datePrefix) + "_";
        if (!stem.starts_with(prefix)) {
            return std::nullopt;
        }

        uint32_t index = 0;
        const std::string_view number(stem.data() + prefix.size(), stem.size() - prefix.size());
        if (number.empty()) {
            return std::nullopt;
        }

        for (char c : number) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return std::nullopt;
            }
            index = index * 10 + static_cast<uint32_t>(c - '0');
        }
        return index;
    }
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
    const auto now = std::chrono::system_clock::now();
    const std::string datePrefix = std::format("{:%Y-%m-%d}", std::chrono::floor<std::chrono::days>(now));

    uint32_t nextIndex = 1;
    std::error_code fsError;
    for (std::filesystem::directory_iterator it(outputDirectory_, fsError), end; !fsError && it != end; it.increment(fsError)) {
        if (fsError || !it->is_regular_file(fsError)) {
            continue;
        }
        if (const std::optional<uint32_t> index = parseDailyCaptureIndex(it->path(), datePrefix)) {
            nextIndex = std::max(nextIndex, *index + 1);
        }
    }

    return outputDirectory_ / std::format("{}_{}.mp4", datePrefix, nextIndex);
}

void CaptureController::resetSessionStats() {
    lastFrameCountSample_ = producer_.capturedFrameCount();
    captureRateAccum_ = 0.0;
    captureFps_ = 0.0f;
    blinkElapsed_ = 0.0;
}
