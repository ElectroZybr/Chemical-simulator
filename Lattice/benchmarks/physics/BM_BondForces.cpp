#include <memory>

#include <benchmark/benchmark.h>

#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/ForceFields/BondForceField.h"
using namespace Lattice;

namespace {

// Цепочка из N атомов углерода с бондами (i, i+1).
// Дистанция ~1.5 близка к равновесию C-C в Morse — бонды не рвутся за время бенча.
// Атомы не двигаются между итерациями (вызывается только BondForceField::compute,
// не integrator), так что геометрия и состав бондов стабильны во всём прогоне.
class BondedChainFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        atomCount_ = static_cast<int>(state.range(0));
        simulation_ = std::make_unique<Simulation>();
        simulation_->createWorld(Vec3f{300.0f, 50.0f, 50.0f});
        simulation_->setSizeBox(Vec3f{300.0f, 50.0f, 50.0f}, 6);

        for (int i = 0; i < atomCount_; ++i) {
            const float x = 10.0f + static_cast<float>(i) * 1.5f;
            simulation_->appendAtomFast(Vec3f{x, 25.0f, 25.0f}, Vec3f{0.0f, 0.0f, 0.0f},
                                        AtomData::Type::C, /*fixed=*/false);
        }
        simulation_->finalizeAtomBatch();

        // Связи (0-1), (1-2), ..., (N-2)-(N-1). Углерод valence=4 — всегда хватает.
        for (int i = 0; i + 1 < atomCount_; ++i) {
            simulation_->addBond(static_cast<size_t>(i), static_cast<size_t>(i + 1));
        }
    }

    void TearDown(benchmark::State&) override { simulation_.reset(); }

protected:
    std::unique_ptr<Simulation> simulation_;
    BondForceField bondForceField_;
    int atomCount_ = 0;
};

} // namespace

// Стоимость одного вызова BondForceField::compute на цепочке N атомов с N-1 бондами.
// Включает: проверку breakage по дистанции (std::erase_if), forceBond (Morse) по
// каждому бонду, applyAngleForces по N-2 углам (центры с двумя соседями).
// @bench_meta {"id":"BondedChainFixture/BondForcesCompute","ru":"Силы цепочки бондов","group":"Симуляция/Бонды"}
BENCHMARK_DEFINE_F(BondedChainFixture, BondForcesCompute)(benchmark::State& state) {
    for (auto _ : state) {
        bondForceField_.compute(simulation_->atoms(), simulation_->bonds(), simulation_->neighborList(),
                                /*allowBondFormation=*/false, /*dt=*/0.01f);
        benchmark::DoNotOptimize(simulation_->atoms().size());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * atomCount_);
}

BENCHMARK_REGISTER_F(BondedChainFixture, BondForcesCompute)
    ->RangeMultiplier(8)
    ->Range(125, 8000)
    ->Args({15625});
