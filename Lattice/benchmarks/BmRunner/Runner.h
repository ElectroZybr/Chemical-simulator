#pragma once

#include <filesystem>

#include "BmRunner/Types.h"

namespace Benchmarks::BmRunner {
    BenchmarkData runBenchmarks(const Config& config, const std::filesystem::path& repoRoot, const std::filesystem::path& benchmarksRoot);
}
