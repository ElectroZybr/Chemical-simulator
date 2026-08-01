#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Benchmarks::BmRunner {
    struct Config {
        std::string filter;
        bool save = false;
        bool list = false;
        int repetitions = 2;
        std::string minTime;
        std::string scene;
        int warmupSteps = 0;
        std::string degradation;
    };

    struct BenchmarkItem {
        std::string runName;
        std::string runType;
        std::string aggregateName;
        std::optional<double> realTime;
        std::optional<double> cpuTime;
        std::optional<double> itemsPerSecond;
    };

    struct RowMetrics {
        std::optional<double> realMedian;
        std::optional<double> realMean;
        std::optional<double> cpuMedian;
        std::optional<double> ips;
        std::optional<double> realCv;
    };

    struct BenchmarkData {
        std::vector<BenchmarkItem> benchmarks;
        std::string rawContextJson;
        std::vector<std::string> rawBenchmarkJsons;
    };

    struct BenchmarkMeta {
        std::string label;
        std::string group;
        std::string sourcePath;
    };
}
