#include <benchmark/benchmark.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "BmRunner/Support.h"
#include "Fixture.h"
#include "Lattice/Engine/pluginLoader.hpp"
#include "Lattice/Log.hpp"

int main(int argc, char** argv) {
    Log::setConsoleMode(Log::ConsoleMode::Quiet);

    const std::filesystem::path benchmarksRoot = Benchmarks::BmRunner::benchmarksRootFromExecutable(argv[0]);
    const std::filesystem::path repoRoot = benchmarksRoot.parent_path().parent_path();
    const std::filesystem::path pluginsPath = repoRoot / "Plugins";

    PluginLoader pluginLoader;
    pluginLoader.load(pluginsPath);
    if (pluginLoader.loadedPlugins().empty()) {
        std::cerr << "[BenchmarkMain] No plugins were loaded from: " << pluginsPath << "\n";
        return 1;
    }

    std::vector<char*> filteredArgs;
    filteredArgs.reserve(static_cast<std::size_t>(argc));
    filteredArgs.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--scene" && i + 1 < argc) {
            Benchmarks::setSelectedScene(Benchmarks::sceneFromString(argv[++i]));
            continue;
        }
        if (arg.rfind("--scene=", 0) == 0) {
            Benchmarks::setSelectedScene(Benchmarks::sceneFromString(arg.substr(8)));
            continue;
        }
        if (arg == "--degradation" && i + 1 < argc) {
            Benchmarks::setSelectedDegradationCriterion(Benchmarks::degradationCriterionFromString(argv[++i]));
            continue;
        }
        if (arg.rfind("--degradation=", 0) == 0) {
            Benchmarks::setSelectedDegradationCriterion(Benchmarks::degradationCriterionFromString(arg.substr(14)));
            continue;
        }
        if (arg == "--warmup-steps" && i + 1 < argc) {
            Benchmarks::setSelectedWarmupSteps(std::stoi(argv[++i]));
            continue;
        }
        if (arg.rfind("--warmup-steps=", 0) == 0) {
            Benchmarks::setSelectedWarmupSteps(std::stoi(arg.substr(15)));
            continue;
        }
        filteredArgs.push_back(argv[i]);
    }

    int filteredArgc = static_cast<int>(filteredArgs.size());
    filteredArgs.push_back(nullptr);
    benchmark::Initialize(&filteredArgc, filteredArgs.data());
    if (benchmark::ReportUnrecognizedArguments(filteredArgc, filteredArgs.data())) {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
