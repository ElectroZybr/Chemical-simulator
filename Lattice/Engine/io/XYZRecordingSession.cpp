#include "XYZRecordingSession.h"

#include "Engine/Simulation.h"

namespace Lattice {

uint32_t XYZRecordingSession::sanitizeStepInterval(uint32_t stepInterval) noexcept { return stepInterval == 0 ? 1u : stepInterval; }

void XYZRecordingSession::resetCountdown() noexcept { stepsUntilNextFrame_ = sanitizeStepInterval(config_.stepInterval); }

void XYZRecordingSession::start(std::string_view outputPath) {
    stream_.Start(outputPath);
    resetCountdown();
}

void XYZRecordingSession::stop() {
    stream_.Stop();
    resetCountdown();
}

void XYZRecordingSession::onSimulationStep(const Simulation& simulation) {
    if (!stream_.IsStreaming()) {
        return;
    }

    if (stepsUntilNextFrame_ > 1) {
        --stepsUntilNextFrame_;
        return;
    }

    stream_.WriteFrame(simulation);
    resetCountdown();
}

void XYZRecordingSession::setConfig(Config config) noexcept {
    config.stepInterval = sanitizeStepInterval(config.stepInterval);
    if (config_.stepInterval == config.stepInterval) {
        return;
    }
    config_ = config;
    resetCountdown();
}

}