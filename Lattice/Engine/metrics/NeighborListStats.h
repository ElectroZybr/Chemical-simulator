#pragma once

#include <array>
#include <cstddef>

class NeighborList;

class NeighborListStats {
public:
    void reset() {
        rebuildCount_ = 0;
        rebuildIntervalsSum_ = 0;
        lastRebuildStep_ = -1;
        recentRebuildIntervals_.fill(0);
        recentRebuildCount_ = 0;
        recentRebuildCursor_ = 0;
        lastRebuildTimeMs_ = 0.0f;
        averageRebuildTimeMs_ = 0.0f;
        maxRebuildTimeMs_ = 0.0f;
    }

    void recordRebuild(int simStep, float rebuildTimeMs) {
        if (lastRebuildStep_ >= 0 && simStep >= lastRebuildStep_) {
            const size_t interval = static_cast<size_t>(simStep - lastRebuildStep_);
            rebuildIntervalsSum_ += interval;
            recentRebuildIntervals_[recentRebuildCursor_] = interval;
            recentRebuildCursor_ = (recentRebuildCursor_ + 1) % kRecentRebuildWindow;
            if (recentRebuildCount_ < kRecentRebuildWindow) {
                ++recentRebuildCount_;
            }
        }

        lastRebuildTimeMs_ = rebuildTimeMs;
        if (rebuildTimeMs > maxRebuildTimeMs_) {
            maxRebuildTimeMs_ = rebuildTimeMs;
        }
        if (rebuildCount_ == 0) {
            averageRebuildTimeMs_ = rebuildTimeMs;
        }
        else {
            averageRebuildTimeMs_ += (rebuildTimeMs - averageRebuildTimeMs_) / static_cast<float>(rebuildCount_ + 1);
        }

        lastRebuildStep_ = simStep;
        ++rebuildCount_;
    }

    [[nodiscard]] size_t rebuildCount() const { return rebuildCount_; }

    // [[nodiscard]] size_t tightness() const { return tightness_; }

    [[nodiscard]] float avgNeighborsPerAtom(const NeighborList& neighborList) const;

    [[nodiscard]] float averageStepsBetweenRebuilds() const {
        if (rebuildCount_ <= 1) {
            return 0.0f;
        }
        return static_cast<float>(rebuildIntervalsSum_) / static_cast<float>(rebuildCount_ - 1);
    }

    [[nodiscard]] float recentAverageStepsBetweenRebuilds() const {
        if (recentRebuildCount_ == 0) {
            return 0.0f;
        }

        size_t sum = 0;
        for (size_t i = 0; i < recentRebuildCount_; ++i) {
            sum += recentRebuildIntervals_[i];
        }
        return static_cast<float>(sum) / static_cast<float>(recentRebuildCount_);
    }

    [[nodiscard]] int stepsSinceLastRebuild(int simStep) const {
        if (lastRebuildStep_ < 0) {
            return simStep;
        }
        return simStep - lastRebuildStep_;
    }

    [[nodiscard]] float lastRebuildTimeMs() const { return lastRebuildTimeMs_; }
    [[nodiscard]] float averageRebuildTimeMs() const { return averageRebuildTimeMs_; }
    [[nodiscard]] float maxRebuildTimeMs() const { return maxRebuildTimeMs_; }

private:
    static constexpr size_t kRecentRebuildWindow = 8;

    size_t rebuildCount_ = 0;
    size_t rebuildIntervalsSum_ = 0;
    int lastRebuildStep_ = -1;
    std::array<size_t, kRecentRebuildWindow> recentRebuildIntervals_{};
    size_t recentRebuildCount_ = 0;
    size_t recentRebuildCursor_ = 0;
    float lastRebuildTimeMs_ = 0.0f;
    float averageRebuildTimeMs_ = 0.0f;
    float maxRebuildTimeMs_ = 0.0f;
};
