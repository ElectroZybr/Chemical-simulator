#include "Profiler.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace {
    bool profileNameEquals(const char* lhs, const char* rhs) noexcept {
        if (lhs == rhs) {
            return true;
        }
        if (lhs == nullptr || rhs == nullptr) {
            return false;
        }
        return std::string_view(lhs) == std::string_view(rhs);
    }

    double smoothingAlpha(double sampleSeconds) noexcept {
        constexpr double kSmoothingWindowSeconds = 0.35;
        if (sampleSeconds <= 0.0) {
            return 1.0;
        }
        return 1.0 - std::exp(-sampleSeconds / kSmoothingWindowSeconds);
    }
}

Profiler& Profiler::instance() {
    static Profiler profiler;
    return profiler;
}

void Profiler::beginFrame() {
    frameStart_ = Clock::now();
    frameActive_ = true;

    for (ProfileEntry& entry : entries_) {
        entry.lastMs = 0.0;
        entry.callCount = 0;
        entry.percentOfFrame = 0.0;
        entry.touchedThisFrame = false;
    }

    activeTreeStack_.clear();
    for (ProfileTreeEntry& entry : treeEntries_) {
        entry.lastMs = 0.0;
        entry.callCount = 0;
        entry.percentOfFrame = 0.0;
        entry.touchedThisFrame = false;
    }
}

void Profiler::endFrame() {
    if (!frameActive_) {
        return;
    }

    frameData_.frameMs = std::chrono::duration<double, std::milli>(Clock::now() - frameStart_).count();
    frameData_.totalTrackedMs = 0.0;
    for (const ProfileTreeEntry& entry : treeEntries_) {
        if (entry.parentIndex == ProfileTreeEntry::kNoParent) {
            frameData_.totalTrackedMs += entry.lastMs;
        }
    }

    const double frameMs = frameData_.frameMs;
    for (ProfileEntry& entry : entries_) {
        entry.percentOfFrame = frameMs > 0.0 ? (entry.lastMs * 100.0 / frameMs) : 0.0;
    }

    const double alpha = smoothingAlpha(frameMs / 1000.0);
    if (!frameData_.hasSmoothedSample) {
        frameData_.smoothedFrameMs = frameData_.frameMs;
        frameData_.smoothedTrackedMs = frameData_.totalTrackedMs;
        frameData_.smoothedTrackedPercent = frameMs > 0.0 ? (frameData_.totalTrackedMs * 100.0 / frameMs) : 0.0;
        frameData_.hasSmoothedSample = true;
    }
    else {
        frameData_.smoothedFrameMs += alpha * (frameData_.frameMs - frameData_.smoothedFrameMs);
        frameData_.smoothedTrackedMs += alpha * (frameData_.totalTrackedMs - frameData_.smoothedTrackedMs);
        const double trackedPercent = frameMs > 0.0 ? (frameData_.totalTrackedMs * 100.0 / frameMs) : 0.0;
        frameData_.smoothedTrackedPercent += alpha * (trackedPercent - frameData_.smoothedTrackedPercent);
    }

    for (ProfileEntry& entry : entries_) {
        if (!entry.hasSmoothedSample) {
            entry.smoothedMs = entry.lastMs;
            entry.smoothedPercentOfFrame = entry.percentOfFrame;
            entry.hasSmoothedSample = true;
        }
        else {
            entry.smoothedMs += alpha * (entry.lastMs - entry.smoothedMs);
            entry.smoothedPercentOfFrame += alpha * (entry.percentOfFrame - entry.smoothedPercentOfFrame);
        }
    }

    for (ProfileTreeEntry& entry : treeEntries_) {
        entry.percentOfFrame = frameMs > 0.0 ? (entry.lastMs * 100.0 / frameMs) : 0.0;
        if (!entry.hasSmoothedSample) {
            entry.smoothedMs = entry.lastMs;
            entry.smoothedPercentOfFrame = entry.percentOfFrame;
            entry.hasSmoothedSample = true;
        }
        else {
            entry.smoothedMs += alpha * (entry.lastMs - entry.smoothedMs);
            entry.smoothedPercentOfFrame += alpha * (entry.percentOfFrame - entry.smoothedPercentOfFrame);
        }
    }

    ++frameData_.frameIndex;
    lastFrameData_ = frameData_;
    frameActive_ = false;
}

void Profiler::reset() {
    frameActive_ = false;
    frameData_ = {};
    lastFrameData_ = {};
    entries_.clear();
    treeEntries_.clear();
    counters_.clear();
    activeTreeStack_.clear();
}

void Profiler::addSample(const char* name, double ms) {
    ProfileEntry& entry = entryFor(name);
    entry.lastMs += ms;
    entry.lastActiveMs = entry.lastMs;
    entry.totalMs += ms;
    entry.maxMs = std::max(entry.maxMs, ms);
    ++entry.callCount;
    ++entry.totalCallCount;
    entry.averageMs = entry.totalCallCount > 0 ? (entry.totalMs / static_cast<double>(entry.totalCallCount)) : 0.0;
    entry.touchedThisFrame = true;
}

size_t Profiler::beginScope(const char* name) noexcept {
    const size_t parentIndex = activeTreeStack_.empty() ? ProfileTreeEntry::kNoParent : activeTreeStack_.back();
    ProfileTreeEntry& entry = treeEntryFor(name, parentIndex);
    const size_t treeIndex = static_cast<size_t>(&entry - treeEntries_.data());
    activeTreeStack_.emplace_back(treeIndex);
    return treeIndex;
}

void Profiler::endScope(size_t treeIndex, const char* name, double ms) noexcept {
    if (!activeTreeStack_.empty() && activeTreeStack_.back() == treeIndex) {
        activeTreeStack_.pop_back();
    }
    else {
        for (auto it = activeTreeStack_.rbegin(); it != activeTreeStack_.rend(); ++it) {
            if (*it == treeIndex) {
                activeTreeStack_.erase(std::next(it).base());
                break;
            }
        }
    }

    addSample(name, ms);

    if (treeIndex >= treeEntries_.size()) {
        return;
    }

    ProfileTreeEntry& entry = treeEntries_[treeIndex];
    entry.lastMs += ms;
    entry.lastActiveMs = entry.lastMs;
    entry.totalMs += ms;
    entry.maxMs = std::max(entry.maxMs, ms);
    ++entry.callCount;
    ++entry.totalCallCount;
    entry.averageMs = entry.totalCallCount > 0 ? (entry.totalMs / static_cast<double>(entry.totalCallCount)) : 0.0;
    entry.touchedThisFrame = true;
}

void Profiler::addCount(const char* name, size_t delta) {
    ProfileCounter& counter = counterFor(name);
    counter.pendingCount += delta;
    counter.totalCount += delta;
}

void Profiler::updateRates(double intervalSeconds) {
    if (intervalSeconds <= 0.0) {
        return;
    }

    constexpr double kRateSmoothingWindowSeconds = 0.15;
    const double alpha = 1.0 - std::exp(-intervalSeconds / kRateSmoothingWindowSeconds);
    for (ProfileCounter& counter : counters_) {
        const double instantRate = static_cast<double>(counter.pendingCount) / intervalSeconds;
        if (!counter.hasRateSample) {
            counter.ratePerSecond = instantRate;
            counter.hasRateSample = true;
        }
        else {
            counter.ratePerSecond += alpha * (instantRate - counter.ratePerSecond);
        }
        counter.pendingCount = 0;
    }
}

double Profiler::counterRate(const char* name) const noexcept {
    for (const ProfileCounter& counter : counters_) {
        if (profileNameEquals(counter.name, name)) {
            return counter.ratePerSecond;
        }
    }
    return 0.0;
}

const ProfileEntry* Profiler::findEntry(const char* name) const noexcept {
    for (const ProfileEntry& entry : entries_) {
        if (profileNameEquals(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

double Profiler::lastMs(const char* name) const noexcept {
    const ProfileEntry* entry = findEntry(name);
    return entry != nullptr ? entry->lastMs : 0.0;
}

double Profiler::lastActiveMs(const char* name) const noexcept {
    const ProfileEntry* entry = findEntry(name);
    return entry != nullptr ? entry->lastActiveMs : 0.0;
}

ProfileEntry& Profiler::entryFor(const char* name) {
    for (ProfileEntry& entry : entries_) {
        if (profileNameEquals(entry.name, name)) {
            return entry;
        }
    }

    ProfileEntry entry{};
    entry.name = name;
    entries_.emplace_back(entry);
    return entries_.back();
}

ProfileTreeEntry& Profiler::treeEntryFor(const char* name, size_t parentIndex) {
    if (parentIndex != ProfileTreeEntry::kNoParent) {
        ProfileTreeEntry& parent = treeEntries_[parentIndex];
        for (const size_t childIndex : parent.childIndices) {
            ProfileTreeEntry& child = treeEntries_[childIndex];
            if (child.parentIndex == parentIndex && profileNameEquals(child.name, name)) {
                return child;
            }
        }
    }
    else {
        for (ProfileTreeEntry& entry : treeEntries_) {
            if (entry.parentIndex == ProfileTreeEntry::kNoParent && profileNameEquals(entry.name, name)) {
                return entry;
            }
        }
    }

    ProfileTreeEntry entry{};
    entry.name = name;
    entry.parentIndex = parentIndex;
    treeEntries_.emplace_back(entry);
    const size_t entryIndex = treeEntries_.size() - 1;
    if (parentIndex != ProfileTreeEntry::kNoParent) {
        treeEntries_[parentIndex].childIndices.emplace_back(entryIndex);
    }
    return treeEntries_.back();
}

ProfileCounter& Profiler::counterFor(const char* name) {
    for (ProfileCounter& counter : counters_) {
        if (profileNameEquals(counter.name, name)) {
            return counter;
        }
    }

    ProfileCounter counter{};
    counter.name = name;
    counters_.emplace_back(counter);
    return counters_.back();
}

ProfileScope::ProfileScope(const char* name) noexcept
    : name_(name), treeIndex_(0) {
    Profiler& profiler = Profiler::instance();
    enabled_ = profiler.isFrameActive();
    if (!enabled_) {
        return;
    }

    treeIndex_ = profiler.beginScope(name);
    start_ = Clock::now();
}

ProfileScope::~ProfileScope() {
    if (!enabled_) {
        return;
    }

    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
    Profiler::instance().endScope(treeIndex_, name_, ms);
}
