#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include "BmRunner/Types.h"

namespace Benchmarks::BmRunner {
    BenchmarkData parseBenchmarkJson(const std::string& jsonText);
    std::unordered_map<std::string, RowMetrics> buildRows(const BenchmarkData& data);
    std::optional<BenchmarkData> loadBenchmarkDataIfExists(const std::filesystem::path& path);
    std::string serializeBenchmarkJson(const BenchmarkData& data);
    void writeBenchmarkJson(const std::filesystem::path& path, const BenchmarkData& data);
    std::unordered_map<std::string, BenchmarkMeta> loadBenchMetadata(const std::filesystem::path& benchmarksRoot);
}
