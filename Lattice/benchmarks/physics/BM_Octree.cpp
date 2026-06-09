#include <benchmark/benchmark.h>

#include "Fixture.h"
#include "Engine/NeighborSearch/BarnesHut/Octree.h"
#include "Engine/physics/ForceFields/CoulombForceField.h"

namespace {
    void assignAlternatingCharges(AtomStorage& atoms) {
        for (size_t i = 0; i < atoms.size(); ++i) {
            atoms.charge(i) = (i & 1u) == 0u ? 1.0f : -1.0f;
        }
    }

    void clearForceAndEnergy(AtomStorage& atoms) {
        for (size_t i = 0; i < atoms.size(); ++i) {
            atoms.forceX(i) = 0.0f;
            atoms.forceY(i) = 0.0f;
            atoms.forceZ(i) = 0.0f;
            atoms.energy(i) = 0.0f;
        }
    }
}

// @bench_meta {"id":"Fixture/BarnesHutOctreeBuild","label":"Barnes-Hut Octree Build","group":"Simulation/Long-range Coulomb"}
BENCHMARK_DEFINE_F(Fixture, BarnesHutOctreeBuild)(benchmark::State& state) {
    rebuildScene();
    warmupScene();

    auto& atoms = simulation_->atoms();
    auto& grid = simulation_->world().getGrid();

    for (auto _ : state) {
        OctreeNode root;
        root.build(atoms, grid);
        benchmark::DoNotOptimize(root);
        benchmark::ClobberMemory();
    }

    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, BarnesHutOctreeBuild)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Arg(47);

// @bench_meta {"id":"Fixture/LongRangeCoulomb","label":"Long-range Coulomb","group":"Simulation/Long-range Coulomb"}
BENCHMARK_DEFINE_F(Fixture, LongRangeCoulomb)(benchmark::State& state) {
    rebuildScene();
    warmupScene();

    auto& atoms = simulation_->atoms();
    auto& grid = simulation_->world().getGrid();
    assignAlternatingCharges(atoms);

    OctreeNode root;
    root.build(atoms, grid);

    CoulombForceField coulombForceField;
    constexpr float kTheta = 0.7f;

    for (auto _ : state) {
        clearForceAndEnergy(atoms);

        for (size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
            float forceX = atoms.forceX(atomIndex);
            float forceY = atoms.forceY(atomIndex);
            float forceZ = atoms.forceZ(atomIndex);
            float potentialEnergy = atoms.energy(atomIndex);

            coulombForceField.computeForce(atoms, atomIndex, root, kTheta, forceX, forceY, forceZ, potentialEnergy);

            atoms.forceX(atomIndex) = forceX;
            atoms.forceY(atomIndex) = forceY;
            atoms.forceZ(atomIndex) = forceZ;
            atoms.energy(atomIndex) = potentialEnergy;
        }

        benchmark::DoNotOptimize(atoms.forceX(0));
        benchmark::DoNotOptimize(atoms.energy(0));
        benchmark::ClobberMemory();
    }

    setCounters(state);
}

BENCHMARK_REGISTER_F(Fixture, LongRangeCoulomb)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Arg(47);
