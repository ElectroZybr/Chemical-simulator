#include <algorithm>
#include <gtest/gtest.h>

#include "Benchmarks/BenchmarkScenes.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"

namespace {

float zSpread(const AtomStorage& atoms) {
    if (atoms.empty()) {
        return 0.0f;
    }
    float minZ = atoms.posZ(0);
    float maxZ = atoms.posZ(0);
    for (size_t i = 1; i < atoms.size(); ++i) {
        const float z = atoms.posZ(i);
        minZ = std::min(minZ, z);
        maxZ = std::max(maxZ, z);
    }
    return maxZ - minZ;
}

float ySpread(const AtomStorage& atoms) {
    if (atoms.empty()) {
        return 0.0f;
    }
    float minY = atoms.posY(0);
    float maxY = atoms.posY(0);
    for (size_t i = 1; i < atoms.size(); ++i) {
        const float y = atoms.posY(i);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    return maxY - minY;
}

} // namespace

// Регрессия для двух связанных багов в Benchmarks/BenchmarkScenes.cpp:
//
// 1) switch case SceneKind::IdealCrystal3D диспатчился в buildCrystal2D
//    (copy-paste). Бенчи по IdealCrystal3D измеряли 2D-плоскость.
//
// 2) buildIdealCrystal3D передавал int side в Vec3f-параметр Scenes::hexLattice;
//    неявная конверсия давала Vec3f(side, 0, 0) — 1D-линию атомов вдоль X.
//
// Корректная сцена IdealCrystal3D должна занимать все три оси.
TEST(BenchmarkScenes, IdealCrystal3DBuildsThreeDimensionalVolume) {
    Simulation simulation;
    simulation.createWorld(Vec3f{160.0f, 160.0f, 160.0f});

    Benchmarks::BenchmarkCase bc;
    bc.scene = Benchmarks::SceneKind::IdealCrystal3D;
    bc.atomCount = 1000;
    bc.boxSize = Vec3f{160.0f, 160.0f, 160.0f};
    bc.cellSize = 5;

    Benchmarks::BenchmarkScenes::build(simulation, bc);

    const AtomStorage& atoms = simulation.atoms();
    ASSERT_GT(atoms.size(), 0u) << "IdealCrystal3D scene should produce atoms";

    EXPECT_GT(ySpread(atoms), 1.0f)
        << "IdealCrystal3D должен заполнять Y (не 1D-линия)";
    EXPECT_GT(zSpread(atoms), 1.0f)
        << "IdealCrystal3D должен заполнять Z (не 2D-плоскость)";
}
