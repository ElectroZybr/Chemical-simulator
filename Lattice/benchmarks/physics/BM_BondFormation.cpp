#include <memory>

#include <benchmark/benchmark.h>

#include "Engine/Simulation.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/ForceFields/BondForceField.h"
using namespace Lattice;

namespace {

// Решётка из N атомов углерода с шагом 1.7 (близко к равновесию C-C, чтобы
// большая часть NL-пар попадала в formationDistance). NL построена один раз
// в SetUp. allowBondFormation=true в бенч-цикле — каждый вызов запускает
// formBonds, который для каждой кандидатной NL-пары вызывает Bond::CreateBond
// и проверяет дубликат через линейный скан bonds-list.
class BondFormationFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        atomCount_ = static_cast<int>(state.range(0));
        simulation_ = std::make_unique<Simulation>();
        simulation_->createWorld(glm::vec3{160.0f, 160.0f, 160.0f});
        simulation_->setSizeBox(glm::vec3{160.0f, 160.0f, 160.0f}, 6);

        const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount_))) + 1;
        int placed = 0;
        for (int z = 0; z < side && placed < atomCount_; ++z) {
            for (int y = 0; y < side && placed < atomCount_; ++y) {
                for (int x = 0; x < side && placed < atomCount_; ++x) {
                    const float fx = 10.0f + static_cast<float>(x) * 1.7f;
                    const float fy = 10.0f + static_cast<float>(y) * 1.7f;
                    const float fz = 10.0f + static_cast<float>(z) * 1.7f;
                    simulation_->appendAtomFast(glm::vec3{fx, fy, fz}, glm::vec3{0.0f, 0.0f, 0.0f}, AtomData::Type::C,
                                                /*fixed=*/false);
                    ++placed;
                }
            }
        }
        simulation_->finalizeAtomBatch();
        simulation_->neighborList().build(simulation_->atoms(), simulation_->world());
    }

    void TearDown(benchmark::State&) override { simulation_.reset(); }

protected:
    std::unique_ptr<Simulation> simulation_;
    BondForceField bondForceField_;
    int atomCount_ = 0;
};

} // namespace

// Стоимость BondForceField::compute с allowBondFormation=true. После первого
// вызова большинство возможных бондов сформированы, и в следующих итерациях
// formBonds проходит через NL и для каждой кандидатной пары делает
// Bond::CreateBond, который делает линейный скан bonds-list на дубликат.
// Это и есть точка измерения D2 (адъяcent-list оптимизации dup-check).
// @bench_meta {"id":"BondFormationFixture/BondCompute","ru":"Силы + формирование бондов","group":"Симуляция/Бонды"}
BENCHMARK_DEFINE_F(BondFormationFixture, BondCompute)(benchmark::State& state) {
    for (auto _ : state) {
        bondForceField_.compute(simulation_->atoms(), simulation_->bonds(), simulation_->neighborList(),
                                /*allowBondFormation=*/true, /*dt=*/0.01f);
        benchmark::DoNotOptimize(simulation_->bonds().size());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * atomCount_);
}

BENCHMARK_REGISTER_F(BondFormationFixture, BondCompute)
    ->RangeMultiplier(8)
    ->Range(125, 8000)
    ->Args({15625});
