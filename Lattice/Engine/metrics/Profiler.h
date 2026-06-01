#pragma once

#include <chrono>
#include <cstddef>
#include <limits>
#include <vector>

struct ProfileEntry {
    const char* name = "";
    double lastMs = 0.0;
    double lastActiveMs = 0.0;
    double smoothedMs = 0.0;
    double totalMs = 0.0;
    double averageMs = 0.0;
    double maxMs = 0.0;
    double percentOfFrame = 0.0;
    double smoothedPercentOfFrame = 0.0;
    size_t callCount = 0;
    size_t totalCallCount = 0;
    bool touchedThisFrame = false;
    bool hasSmoothedSample = false;
};

struct ProfilerFrameData {
    double frameMs = 0.0;
    double totalTrackedMs = 0.0;
    double smoothedFrameMs = 0.0;
    double smoothedTrackedMs = 0.0;
    double smoothedTrackedPercent = 0.0;
    size_t frameIndex = 0;
    bool hasSmoothedSample = false;
};

struct ProfileCounter {
    const char* name = "";
    size_t pendingCount = 0;
    size_t totalCount = 0;
    double ratePerSecond = 0.0;
    bool hasRateSample = false;
};

struct ProfileTreeEntry {
    static constexpr size_t kNoParent = std::numeric_limits<size_t>::max();

    const char* name = "";
    size_t parentIndex = kNoParent;
    std::vector<size_t> childIndices{};
    double lastMs = 0.0;
    double lastActiveMs = 0.0;
    double smoothedMs = 0.0;
    double totalMs = 0.0;
    double averageMs = 0.0;
    double maxMs = 0.0;
    double percentOfFrame = 0.0;
    double smoothedPercentOfFrame = 0.0;
    size_t callCount = 0;
    size_t totalCallCount = 0;
    bool touchedThisFrame = false;
    bool hasSmoothedSample = false;
};

class Profiler {
public:
    static Profiler& instance();

    void beginFrame();
    void endFrame();
    void reset();

    void addSample(const char* name, double ms);
    void addCount(const char* name, size_t delta = 1);
    void updateRates(double intervalSeconds);

    [[nodiscard]] bool isFrameActive() const noexcept { return frameActive_; }
    [[nodiscard]] const ProfilerFrameData& frameData() const noexcept { return frameData_; }
    [[nodiscard]] const ProfilerFrameData& lastFrameData() const noexcept { return lastFrameData_; }
    [[nodiscard]] const std::vector<ProfileEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] const std::vector<ProfileTreeEntry>& treeEntries() const noexcept { return treeEntries_; }
    [[nodiscard]] const ProfileEntry* findEntry(const char* name) const noexcept;
    [[nodiscard]] double lastMs(const char* name) const noexcept;
    [[nodiscard]] double lastActiveMs(const char* name) const noexcept;
    [[nodiscard]] double counterRate(const char* name) const noexcept;
    size_t beginScope(const char* name) noexcept;
    void endScope(size_t treeIndex, const char* name, double ms) noexcept;

private:
    using Clock = std::chrono::steady_clock;

    Profiler() = default;

    Clock::time_point frameStart_{};
    bool frameActive_ = false;

    ProfilerFrameData frameData_{};
    ProfilerFrameData lastFrameData_{};
    std::vector<ProfileEntry> entries_{};
    std::vector<ProfileTreeEntry> treeEntries_{};
    std::vector<ProfileCounter> counters_{};
    std::vector<size_t> activeTreeStack_{};

    ProfileEntry& entryFor(const char* name);
    ProfileTreeEntry& treeEntryFor(const char* name, size_t parentIndex);
    ProfileCounter& counterFor(const char* name);
};

class ProfileScope {
public:
    explicit ProfileScope(const char* name) noexcept;
    ~ProfileScope();

    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

private:
    using Clock = std::chrono::steady_clock;

    const char* name_;
    bool enabled_ = false;
    size_t treeIndex_;
    Clock::time_point start_;
};

#define PROFILE_SCOPE(name) ::ProfileScope profileScope##__LINE__(name)
#define PROFILE_FUNC() ::ProfileScope profileScope##__LINE__(__FUNCTION__)
