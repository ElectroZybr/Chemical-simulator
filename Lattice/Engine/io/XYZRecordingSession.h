#pragma once

#include <cstdint>
#include <string_view>

#include "Engine/io/xyzStream.h"
 
namespace Lattice {
    
class Simulation;

class XYZRecordingSession {
public:
    struct Config {
        uint32_t stepInterval = 1;
    };

    void start(std::string_view outputPath);
    void stop();
    void onSimulationStep(const Simulation& simulation);

    void setConfig(Config config) noexcept;
    [[nodiscard]] const Config& config() const noexcept { return config_; }
    [[nodiscard]] uint32_t stepInterval() const noexcept { return config_.stepInterval; }

    [[nodiscard]] bool isRecording() const noexcept { return stream_.IsStreaming(); }
    [[nodiscard]] uint64_t frameCount() const noexcept { return stream_.FrameCount(); }
    [[nodiscard]] float fps() const noexcept { return stream_.FPS(); }

private:
    static uint32_t sanitizeStepInterval(uint32_t stepInterval) noexcept;
    void resetCountdown() noexcept;

    Config config_{};
    uint32_t stepsUntilNextFrame_ = 1;
    xyzStream stream_;
};
}