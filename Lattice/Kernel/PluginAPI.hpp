#pragma once

#include <vector>
#include <string>
#include <filesystem>

#include "Lattice/Kernel/Registry.hpp"

namespace Lattice {
struct Version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;

    auto operator<=>(const Version&) const = default;

    std::string str() {
        return std::format("{}.{}.{}", major, minor, patch);
    }
};

struct VersionRange {
    std::optional<Version> min;
    std::optional<Version> max;
    std::string requirement;

    bool contains(const Version& version) const {
        if (min && version < *min)
            return false;

        if (max && version >= *max)
            return false;

        return true;
    }

    static Version parseVersion(std::string_view str) {
        Version version{};
        std::sscanf(
            str.data(),
            "%hhu.%hhu.%hhu",
            &version.major,
            &version.minor,
            &version.patch
        );

        return version;
    }

    static VersionRange parse(std::string_view str) {
        VersionRange range;

        range.requirement = str;

        if (str.starts_with(">=")) {
            range.min = parseVersion(str.substr(2));
        }
        else if (str.starts_with("<")) {
            range.max = parseVersion(str.substr(1));
        }
        else {
            // точная версия
            Version v = parseVersion(str);
            range.min = v;

            Version next = v;
            next.patch++;

            range.max = next;
        }

        return range;
    }
};

struct PluginDependency {
    std::string id;
    VersionRange requirement;
};

struct PluginManifest {
    std::string id;
    std::string name;
    Version version;
    uint32_t kernelApiVersion;

    std::vector<PluginDependency> dependencies;
};

enum class LoadStatus {
    NonChecked,
    // dependency check
    Valid,
    MissingDependency,
    IncompatibleVersion,
    DependencyCycle,
    // loading
    Queued,
    Loaded,
    Failed
};

struct PluginCandidate {
    std::filesystem::path path;
    PluginManifest manifest;
    LoadStatus status = LoadStatus::NonChecked;
    std::vector<std::string> providedAPIs;
};

using PluginRegisterFn = bool(*)(Registry*);
using PluginShutdownFn = void(*)();
}