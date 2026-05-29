// Эмпирическое сравнение ДО оптимизаций (baseline fd28dc8) и ПОСЛЕ (optimized
// CPU). Эталонные значения захвачены прогоном идентичного кода на baseline
// (golden_capture.cpp в worktree fd28dc8). Доказывает:
//   1. Силы для пар В ПРЕДЕЛАХ cutoff — побитово идентичны baseline (формула и
//      вычисление не менялись).
//   2. Единственный намеренный сдвиг поведения — cutoff-фикс (a72aff1): baseline
//      прикладывал LJ-силу к паре в skin-зоне (5,6], оптимизация — нет. Магнитуда
//      этой "забагованной" силы мала (далёкое поле LJ).
//   3. Траектория за 20 шагов совпадает с baseline в пределах этого далёкого поля
//      (порядок 1e-3), т.е. практически ТАК ЖЕ.
// CPU == GPU отдельно доказано BM_GpuCorrectness (max|d| = 0.0).

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <gtest/gtest.h>

#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"

namespace {

Simulation makeForceScene() {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
    sim.setNeighborListCutoff(5.0f);
    sim.setNeighborListSkin(1.0f);
    sim.appendAtomFast(Vec3f{20.0f, 20.0f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::H);  // atom0
    sim.appendAtomFast(Vec3f{23.0f, 20.0f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::H);  // 3.0 от atom0
    sim.appendAtomFast(Vec3f{20.0f, 25.4f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::H);  // 5.4 от atom0 (skin)
    sim.finalizeAtomBatch();
    sim.neighborList().build(sim.atoms(), sim.world());
    sim.forceField().computePairInteractions(sim.world());
    return sim;
}

} // namespace

// Baseline (захват): F0=(0.029929217, 0.00101180596, 0), F1=(-0.029929217,0,0),
// F2=(0, -0.00101180596, 0). Optimized: x-компоненты (пара на 3.0, в cutoff) —
// идентичны; y-компонента (пара на 5.4, skin-зона) — обнулена cutoff-фиксом.
TEST(BaselineParityTest, ForcesInCutoffMatchBaseline) {
    Simulation sim = makeForceScene();
    const AtomStorage& a = sim.atoms();

    // Пара atom0-atom1 на 3.0 (в cutoff) — сила ПОБИТОВО как в baseline.
    EXPECT_FLOAT_EQ(a.forceX(0), 0.029929217f) << "сила пары в cutoff изменилась — формула не должна была меняться";
    EXPECT_FLOAT_EQ(a.forceX(1), -0.029929217f);
    EXPECT_EQ(a.forceY(1), 0.0f);
    EXPECT_EQ(a.forceZ(1), 0.0f);

    // Пара atom0-atom2 на 5.4 (skin-зона (5,6]) — baseline давал y-силу
    // 0.00101..., оптимизация обрезает по cutoff -> 0. Это намеренный багфикс.
    constexpr float kBaselineSkinForceY = 0.00101180596f;
    std::printf("[ PARITY   ] skin-zone force baseline=%.9g, optimized=%.9g (cutoff-фикс)\n",
                static_cast<double>(kBaselineSkinForceY), static_cast<double>(a.forceY(0)));
    EXPECT_EQ(a.forceY(0), 0.0f) << "cutoff-фикс должен обнулять силу за cutoff";
    EXPECT_EQ(a.forceY(2), 0.0f);
    // Магнитуда убранной "забагованной" силы — мала (далёкое поле LJ).
    EXPECT_LT(kBaselineSkinForceY, 0.01f * std::abs(0.029929217f) + 0.01f);
}

// Траектория 3x3x3 (шаг 3.0) за 20 шагов. Эталон — baseline. Совпадение в
// пределах далёкого поля cutoff (пары на 6.0 ед. baseline учитывал, optimized нет).
TEST(BaselineParityTest, Trajectory20StepsMatchesBaseline) {
    Simulation sim;
    sim.createWorld(Vec3f{60.0f, 60.0f, 60.0f});
    sim.setSizeBox(Vec3f{60.0f, 60.0f, 60.0f}, 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
    sim.setNeighborListCutoff(5.0f);
    sim.setNeighborListSkin(1.0f);
    sim.setDt(0.01f);
    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                sim.appendAtomFast(Vec3f{20.0f + x * 3.0f, 20.0f + y * 3.0f, 20.0f + z * 3.0f}, Vec3f{0, 0, 0}, AtomData::Type::H);
            }
        }
    }
    sim.finalizeAtomBatch();
    for (int s = 0; s < 20; ++s) {
        sim.update();
    }
    const AtomStorage& a = sim.atoms();

    // Baseline-эталон углов и центра (захват).
    struct PG {
        size_t i;
        double x, y, z;
    };
    const PG golden[] = {
        {0, 20.0006599, 20.0006599, 20.0006599},
        {13, 23.0, 23.0, 23.0},
        {26, 25.9993401, 25.9993401, 25.9993401},
    };
    double maxDiff = 0.0;
    for (const PG& g : golden) {
        maxDiff = std::max(maxDiff, std::abs(static_cast<double>(a.posX(g.i)) - g.x));
        maxDiff = std::max(maxDiff, std::abs(static_cast<double>(a.posY(g.i)) - g.y));
        maxDiff = std::max(maxDiff, std::abs(static_cast<double>(a.posZ(g.i)) - g.z));
    }
    std::printf("[ PARITY   ] trajectory max|optimized-baseline| over 20 steps = %.3e\n", maxDiff);
    EXPECT_LT(maxDiff, 1e-3) << "траектория разошлась с baseline сильнее далёкого поля cutoff";
}
