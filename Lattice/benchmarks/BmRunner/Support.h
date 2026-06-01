#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "BmRunner/Types.h"

namespace Benchmarks::BmRunner {
    [[noreturn]] void die(const std::string& message);

    std::string readTextFile(const std::filesystem::path& path);
    std::string quoteShell(const std::string& value);
    std::string sceneLabel(std::string_view key);

    std::filesystem::path benchmarksRootFromExecutable(const char* argv0);
    std::filesystem::path resolveBenchmarkBinary(const std::filesystem::path& repoRoot);
    std::filesystem::path resultsDir(const std::filesystem::path& benchmarksRoot);

    void ensureResultsDir(const std::filesystem::path& dir);
    std::vector<std::filesystem::path> savedResults(const std::filesystem::path& dir);
    std::string formatTimestamp(const std::filesystem::path& path);

    Config parseArgs(int argc, char** argv);

    std::string shortBenchId(std::string_view fullName);
    std::string metaLookupId(std::string_view runName);
    std::string benchArg(std::string_view fullName);
    int atomCountForSceneKey(std::string_view key, int sceneExtent);
}
