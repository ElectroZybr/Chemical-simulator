#pragma once

#include <filesystem>
#include <string_view>
#include <unordered_map>

#include "BmRunner/Types.h"

namespace Benchmarks::BmRunner {
    void printResultsTable(const BenchmarkData& data,
                           const std::unordered_map<std::string, BenchmarkMeta>& metadata,
                           const std::filesystem::path& resultsPath,
                           std::string_view sceneKey);
}
