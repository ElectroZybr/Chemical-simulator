#include <benchmark/benchmark.h>

#include <string>
#include <vector>

#ifdef LATTICELAB_USE_TBB
#include <cstdlib>
#include <optional>
#include <tbb/global_control.h>
#endif

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

    // Диагностический хук: env LL_TBB_THREADS=N ограничивает TBB N потоками
    // (max_allowed_parallelism). Нужно для замера single-thread на параллельном
    // коде БЕЗ affinity-pin — pin загоняет все воркеры на одно ядро и даёт ложный
    // спин-трэшинг. Не задан -> поведение прежнее (все ядра).
#ifdef LATTICELAB_USE_TBB
    std::optional<tbb::global_control> tbbLimit;
    if (const char* t = std::getenv("LL_TBB_THREADS")) {
        const int n = std::atoi(t);
        if (n > 0)
            tbbLimit.emplace(tbb::global_control::max_allowed_parallelism,
                             static_cast<std::size_t>(n));
    }
#endif

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
