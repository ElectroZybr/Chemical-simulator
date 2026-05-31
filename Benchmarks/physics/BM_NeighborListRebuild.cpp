#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/SimulationFixture.h"

// @bench_meta {"id":"SimulationFixture/NeighborListRebuildOnly","ru":"Перестройка NeighborList","group":"Симуляция/Сетка и соседи"}
BENCHMARK_DEFINE_F(SimulationFixture, NeighborListRebuildOnly)(benchmark::State& state) {
    rebuildScene();

    auto& atoms = simulation_->atoms();
    auto& grid = simulation_->world().getGrid();
    grid.rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());

    for (auto _ : state) {
        simulation_->neighborList().build(simulation_->atoms(), simulation_->world());
        benchmark::DoNotOptimize(simulation_->neighborList().pairStorageSize());
        benchmark::ClobberMemory();
    }

    setCounters(state);
}

BENCHMARK_REGISTER_F(SimulationFixture, NeighborListRebuildOnly)
    ->RangeMultiplier(8)
    ->Range(Benchmarks::kAtomMin, Benchmarks::kAtomMax)
    ->Args({15625})   // 25^3
    ->Args({103823}); // 47^3
