#include <benchmark/benchmark.h>

#include <string>
#include <vector>

#include "Fixture.h"
using namespace Lattice;

int main(int argc, char** argv) {
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
