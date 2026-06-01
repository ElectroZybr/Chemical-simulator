#include "xyzStream.h"

#include "Engine/Simulation.h"

#include <fstream>

void xyzStream::Start(std::string_view path) {
    path_ = std::string(path);
    std::ofstream file(path_, std::ios::trunc);
    if (!file.is_open()) {
        isStreaming = false;
        return;
    }
    isStreaming = true;
    frameCount_ = 0;
    sampleFrameCount_ = 0;
    fps_ = 0.0f;
    sampleStart_ = Clock::now();
}

void xyzStream::WriteFrame(const Lattice::Simulation& simulation) {
    if (!isStreaming || path_.empty()) {
        return;
    }
    std::ofstream file(path_.data(), std::ios::app);
    if (!file.is_open()) {
        return;
    }

    const size_t atomCount = simulation.atoms().size();
    file << atomCount << "\n";
    file << "Step: " << simulation.world().getSimStep() << ", Time: " << simulation.world().getSimTimeNs() << " ns\n";
    for (size_t i = 0; i < atomCount; ++i) {
        const Vec3f pos = simulation.atoms().pos(i);
        file << AtomData::symbol(simulation.atoms().type(i)) << " " << pos.x << " " << pos.y << " " << pos.z << "\n";
    }

    ++frameCount_;
    ++sampleFrameCount_;

    const auto now = Clock::now();
    const float elapsed = std::chrono::duration<float>(now - sampleStart_).count();
    if (elapsed >= 0.25f) {
        fps_ = elapsed > 0.0f ? static_cast<float>(sampleFrameCount_) / elapsed : 0.0f;
        sampleFrameCount_ = 0;
        sampleStart_ = now;
    }
}

void xyzStream::Stop() {
    isStreaming = false;
    path_.clear();
    sampleFrameCount_ = 0;
    fps_ = 0.0f;
}
