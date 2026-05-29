// Correctness-гейт переноса физики на GPU: GPU-траектория должна совпасть с
// CPU velocity Verlet в пределах fp-tolerance. Без этого «быстро» бессмысленно.
//
// Сцена: малая решётка с малыми начальными скоростями, gravity=0, атомы глубоко
// внутри box (confine — no-op на обеих сторонах), LJ-only, bonds off. Прогон
// короткий (kSteps), скорости малы → ни CPU, ни GPU не триггерят NL rebuild,
// так что обе стороны держат один и тот же initial NL. Это изолирует
// корректность integrator+LJ от логики rebuild (тестируется отдельно).
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Бросает при расхождении выше tolerance — прогон падает.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <vector>

#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"

namespace {

constexpr int kSteps = 50;
constexpr float kDt = 0.01f;
constexpr float kAccelDamping = 0.9f;

// Заполняет Simulation решёткой N атомов H с малыми seeded-скоростями.
void fillScene(Simulation& sim, int atomCount, float worldSize, float spacing) {
    std::mt19937 rng(424242);
    std::uniform_real_distribution<float> vel(-0.3f, 0.3f);
    const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount))) + 1;
    int placed = 0;
    for (int z = 0; z < side && placed < atomCount; ++z) {
        for (int y = 0; y < side && placed < atomCount; ++y) {
            for (int x = 0; x < side && placed < atomCount; ++x) {
                sim.appendAtomFast(Vec3f{20.0f + x * spacing, 20.0f + y * spacing, 20.0f + z * spacing},
                                   Vec3f{vel(rng), vel(rng), vel(rng)}, AtomData::Type::H, false);
                ++placed;
            }
        }
    }
    sim.finalizeAtomBatch();
}

void runCorrectness(benchmark::State& state) {
    benchmarkDevice();
    const int atomCount = static_cast<int>(state.range(0));
    const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount))) + 1;
    const float spacing = 3.0f;
    const float worldSize = side * spacing + 40.0f;

    // --- CPU reference ---
    Simulation cpu;
    cpu.createWorld(Vec3f{worldSize, worldSize, worldSize});
    cpu.setSizeBox(cpu.world().getWorldSize(), 6);
    cpu.setLJEnabled(true);
    cpu.setCoulombEnabled(false);
    cpu.setBondFormationEnabled(false);
    cpu.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
    cpu.setDt(kDt);
    cpu.setAccelDamping(kAccelDamping);
    fillScene(cpu, atomCount, worldSize, spacing);

    // --- GPU: тот же стартовый стейт ---
    Simulation gpu;
    gpu.createWorld(Vec3f{worldSize, worldSize, worldSize});
    gpu.setSizeBox(gpu.world().getWorldSize(), 6);
    gpu.setLJEnabled(true);
    gpu.setCoulombEnabled(false);
    gpu.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
    gpu.neighborList().setMode(NeighborListMode::Full);
    fillScene(gpu, atomCount, worldSize, spacing);
    gpu.neighborList().build(gpu.atoms(), gpu.world());

    LJForceField lj;
    GpuResidentPhysics grp;
    // gravity=0 (как CPU reference): атомы глубоко внутри box, wall-kernel прибавит
    // ровно 0 → LJ-only паритет сохраняется (Q6-регрессия остаётся зелёной).
    grp.uploadFromCpu(gpu.atoms(), gpu.neighborList(), lj, worldSize, worldSize, worldSize, 0.0f, 0.0f, 0.0f, /*ljEnabled=*/true);

    // Прогон.
    for (auto _ : state) {
        Simulation cpuRun;
        cpuRun.createWorld(Vec3f{worldSize, worldSize, worldSize});
        cpuRun.setSizeBox(cpuRun.world().getWorldSize(), 6);
        cpuRun.setLJEnabled(true);
        cpuRun.setCoulombEnabled(false);
        cpuRun.setBondFormationEnabled(false);
        cpuRun.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
        cpuRun.setDt(kDt);
        cpuRun.setAccelDamping(kAccelDamping);
        fillScene(cpuRun, atomCount, worldSize, spacing);
        for (int s = 0; s < kSteps; ++s) {
            cpuRun.updateAll();
        }

        Simulation gpuRun;
        gpuRun.createWorld(Vec3f{worldSize, worldSize, worldSize});
        gpuRun.setSizeBox(gpuRun.world().getWorldSize(), 6);
        gpuRun.setLJEnabled(true);
        gpuRun.setCoulombEnabled(false);
        gpuRun.neighborList().setMode(NeighborListMode::Full);
        fillScene(gpuRun, atomCount, worldSize, spacing);
        gpuRun.neighborList().build(gpuRun.atoms(), gpuRun.world());
        GpuResidentPhysics g;
        g.uploadFromCpu(gpuRun.atoms(), gpuRun.neighborList(), lj, worldSize, worldSize, worldSize, 0.0f, 0.0f, 0.0f, /*ljEnabled=*/true);
        for (int s = 0; s < kSteps; ++s) {
            g.step(kDt, kAccelDamping);
        }
        g.downloadToCpu(gpuRun.atoms(), true);

        // Сравнение позиций по индексу (порядок атомов идентичен — fillScene
        // детерминирован).
        const AtomStorage& a = cpuRun.atoms();
        const AtomStorage& b = gpuRun.atoms();
        double maxAbs = 0.0;
        double maxRel = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            for (int c = 0; c < 3; ++c) {
                const float av = (c == 0) ? a.posX(i) : (c == 1) ? a.posY(i) : a.posZ(i);
                const float bv = (c == 0) ? b.posX(i) : (c == 1) ? b.posY(i) : b.posZ(i);
                const double diff = std::abs(static_cast<double>(av) - bv);
                maxAbs = std::max(maxAbs, diff);
                const double scale = std::max(std::abs(av), std::abs(bv));
                if (scale > 1e-6) {
                    maxRel = std::max(maxRel, diff / scale);
                }
            }
        }
        std::printf("[ CORRECT  ] N=%d steps=%d: max|dCPU-GPU| = %.3e abs, %.3e rel\n", atomCount, kSteps, maxAbs, maxRel);
        state.counters["max_abs"] = maxAbs;
        state.counters["max_rel"] = maxRel;

        // Tolerance: fp32 accumulation за 50 шагов + разный порядок суммирования
        // LJ (Half на CPU vs Full на GPU) → расхождение в LSB-диапазоне. Порог
        // консервативный: абсолютное смещение позиции < 1e-2 u (атомы движутся
        // на единицы u за прогон, так что это <1% от движения).
        if (maxAbs > 1e-2) {
            throw std::runtime_error("GPU trajectory diverged from CPU beyond tolerance");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuCorrectness/VerletMatchesCpu","ru":"GPU Verlet == CPU","group":"Симуляция/GPU"}
void BM_GpuCorrectness_VerletMatchesCpu(benchmark::State& state) { runCorrectness(state); }

BENCHMARK(BM_GpuCorrectness_VerletMatchesCpu)->Arg(512)->Unit(benchmark::kMicrosecond);
