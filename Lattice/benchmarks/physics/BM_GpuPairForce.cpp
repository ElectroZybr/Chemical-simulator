#include <memory>

#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h" // тащит benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/gpu/GpuPairForceCompute.h"
using namespace Lattice;

namespace {

// Кубическая решётка атомов H в боксе 300^3, шаг 3.0 (как у render-фикстуры).
// NL переключается на Full для совместимости с GPU-контрактом (компьют пишет
// только в central forces, парность держится глобально через два прохода
// одной пары).
class GpuPairForceFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        atomCount_ = static_cast<int>(state.range(0));

        // Поднимаем WGPU device (idempotent — RendererFixtureBase делает то же).
        benchmarkDevice();

        simulation_ = std::make_unique<Simulation>();
        simulation_->createWorld(glm::vec3{300.0f, 300.0f, 300.0f});
        simulation_->setSizeBox(glm::vec3{300.0f, 300.0f, 300.0f}, 6);
        simulation_->setLJEnabled(true);
        simulation_->setCoulombEnabled(false);
        simulation_->neighborList().setMode(NeighborListMode::Full);

        const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount_))) + 1;
        int placed = 0;
        for (int z = 0; z < side && placed < atomCount_; ++z) {
            for (int y = 0; y < side && placed < atomCount_; ++y) {
                for (int x = 0; x < side && placed < atomCount_; ++x) {
                    simulation_->appendAtomFast(
                        glm::vec3{10.0f + static_cast<float>(x) * 3.0f, 10.0f + static_cast<float>(y) * 3.0f,
                              10.0f + static_cast<float>(z) * 3.0f},
                        glm::vec3{0.0f, 0.0f, 0.0f}, AtomData::Type::H, /*fixed=*/false);
                    ++placed;
                }
            }
        }
        simulation_->finalizeAtomBatch();
        simulation_->neighborList().build(simulation_->atoms(), simulation_->world());

        compute_ = std::make_unique<GpuPairForceCompute>();
    }

    void TearDown(benchmark::State&) override {
        compute_.reset();
        simulation_.reset();
    }

protected:
    std::unique_ptr<Simulation> simulation_;
    std::unique_ptr<GpuPairForceCompute> compute_;
    LJForceField ljForceField_;
    int atomCount_ = 0;
};

} // namespace

// @bench_meta {"id":"GpuPairForceFixture/Compute","ru":"GPU LJ pair force","group":"Симуляция/GPU"}
BENCHMARK_DEFINE_F(GpuPairForceFixture, Compute)(benchmark::State& state) {
    for (auto _ : state) {
        compute_->compute(simulation_->atoms(), simulation_->neighborList(), ljForceField_);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * atomCount_);
}

BENCHMARK_REGISTER_F(GpuPairForceFixture, Compute)
    ->RangeMultiplier(8)
    ->Range(1000, 8000)
    ->Args({15625})
    ->Args({103823});
