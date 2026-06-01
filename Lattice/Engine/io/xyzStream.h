#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>

namespace Lattice {
    class Simulation;
}

class xyzStream {
public:
    void Start(std::string_view path);
    void WriteFrame(const Lattice::Simulation& simulation);
    void Stop();
    [[nodiscard]] bool IsStreaming() const noexcept { return isStreaming; }
    [[nodiscard]] const std::string& Path() const noexcept { return path_; }
    [[nodiscard]] uint64_t FrameCount() const noexcept { return frameCount_; }
    [[nodiscard]] float FPS() const noexcept { return fps_; }

private:
    using Clock = std::chrono::steady_clock;

    std::string path_ = "";
    bool isStreaming = false;
    uint64_t frameCount_ = 0;
    uint64_t sampleFrameCount_ = 0;
    float fps_ = 0.0f;
    Clock::time_point sampleStart_{};
};
