#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Lattice/Kernel/DynamicLibrary.hpp"
#include "Lattice/Kernel/Registry.hpp"

namespace Lattice {
    
struct Version {
    uint8_t major = 0;
    uint8_t minor = 0;
    uint8_t patch = 0;

    auto operator<=>(const Version&) const = default;

    std::string str() const {
        return std::format("{}.{}.{}", major, minor, patch);
    }
};

inline constexpr Version kernelApi{1, 0, 0};

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
    Version kernelApi{};

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

using PluginRegisterFn = bool(*)(Registry&);
using PluginShutdownFn = void(*)();

struct Plugin {
    std::filesystem::path path;
    PluginManifest manifest;
    LoadStatus status = LoadStatus::NonChecked;

    DynamicLibrary* library = nullptr;
    PluginShutdownFn shutdown = nullptr;

    std::vector<std::string> providedAPIs;
};
}