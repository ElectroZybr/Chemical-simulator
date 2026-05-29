// Кулоновское парное взаимодействие. Оптимизации добавили writeNeighbor-параметр
// (как в LJ) для Full-режима NL. Покрываем: корректность силы пары (знак +
// магнитуда по формуле), Newton-3, и эквивалентность Half (writeNeighbor=true)
// и Full (writeNeighbor=false) — суммарная сила на атом одинакова.

#include <cmath>

#include <gtest/gtest.h>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/ForceFields/CoulombForceField.h"

namespace {

// Na (+1) и Cl (-1) на расстоянии 3 по оси x. LJ выключен — изолируем Кулон.
Simulation makeIonPair(NeighborListMode mode) {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, 6);
    sim.setLJEnabled(false);
    sim.setCoulombEnabled(true);
    sim.setBondFormationEnabled(false);
    sim.neighborList().setMode(mode);
    sim.appendAtomFast(Vec3f{20.0f, 20.0f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::Na);
    sim.appendAtomFast(Vec3f{23.0f, 20.0f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::Cl);
    sim.finalizeAtomBatch();
    sim.neighborList().build(sim.atoms(), sim.world());
    sim.forceField().computePairInteractions(sim.world());
    return sim;
}

} // namespace

// Противоположные заряды притягиваются: Na (+) тянется к Cl (+x). Магнитуда по
// формуле kCoulomb*q1*q2/r^2 = 140.399645 * 1 * 1 / 9.
TEST(CoulombTest, IonPairForceAndNewton3) {
    Simulation sim = makeIonPair(NeighborListMode::Half);
    const AtomStorage& a = sim.atoms();

    const float expectedMag = CoulombForceField::kCoulombEvAngstrom * 1.0f * 1.0f / 9.0f; // r^2 = 9
    EXPECT_GT(a.forceX(0), 0.0f) << "Na должен притягиваться к Cl (+x)";
    EXPECT_NEAR(static_cast<double>(a.forceX(0)), static_cast<double>(expectedMag), 1e-2);
    // Newton-3.
    EXPECT_FLOAT_EQ(a.forceX(1), -a.forceX(0));
    EXPECT_EQ(a.forceY(0), 0.0f);
    EXPECT_EQ(a.forceZ(0), 0.0f);
}

// Half (writeNeighbor=true, парность через запись соседу) и Full
// (writeNeighbor=false, каждая сторона считает свою) дают одинаковую суммарную
// силу на атом.
TEST(CoulombTest, HalfFullProduceSameForce) {
    Simulation half = makeIonPair(NeighborListMode::Half);
    Simulation full = makeIonPair(NeighborListMode::Full);

    for (size_t i = 0; i < 2; ++i) {
        EXPECT_FLOAT_EQ(half.atoms().forceX(i), full.atoms().forceX(i)) << "atom " << i;
        EXPECT_FLOAT_EQ(half.atoms().forceY(i), full.atoms().forceY(i));
        EXPECT_FLOAT_EQ(half.atoms().forceZ(i), full.atoms().forceZ(i));
    }
}

namespace {

// Сцена для комбинированного режима: Na(+1), Cl(-1) и НЕЙТРАЛЬНЫЙ H(0) рядом,
// оба тумблера (LJ + Coulomb) включены.
Simulation makeMixed(NeighborListMode mode) {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(true);
    sim.setBondFormationEnabled(false);
    sim.neighborList().setMode(mode);
    sim.appendAtomFast(Vec3f{20.0f, 20.0f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::Na); // +1
    sim.appendAtomFast(Vec3f{23.0f, 20.0f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::Cl); // -1
    sim.appendAtomFast(Vec3f{20.0f, 23.0f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::H);  // 0
    sim.finalizeAtomBatch();
    sim.neighborList().build(sim.atoms(), sim.world());
    sim.forceField().computePairInteractions(sim.world());
    return sim;
}

} // namespace

// Комбинированный режим LJ+Coulomb (диспатч <true,true>) — нормальный
// пользовательский режим (два независимых тумблера), не покрытый ранее ни одним
// тестом. Ключевая ветка: нейтральный атом (charge==0) в смешанном режиме НЕ
// выходит рано (early-return только при !UseLJ), а досчитывает LJ.
TEST(CoulombTest, CombinedLjCoulombNeutralStillGetsLj) {
    Simulation half = makeMixed(NeighborListMode::Half);

    // Нейтральный H (атом 2) обязан получить ненулевую LJ-силу от соседей.
    const double hMag = std::abs(static_cast<double>(half.atoms().forceX(2))) +
                        std::abs(static_cast<double>(half.atoms().forceY(2))) +
                        std::abs(static_cast<double>(half.atoms().forceZ(2)));
    EXPECT_GT(hMag, 0.0) << "нейтральный атом в LJ+Coulomb должен получить LJ-силу (ранний выход не срабатывает)";

    // Newton-3: суммарная сила замкнутой системы ~0.
    double sx = 0, sy = 0, sz = 0;
    for (size_t i = 0; i < 3; ++i) {
        sx += half.atoms().forceX(i);
        sy += half.atoms().forceY(i);
        sz += half.atoms().forceZ(i);
    }
    EXPECT_NEAR(sx, 0.0, 1e-2);
    EXPECT_NEAR(sy, 0.0, 1e-2);
    EXPECT_NEAR(sz, 0.0, 1e-2);

    // Half == Full в комбинированном режиме (writeNeighbor-симметрия для обеих сил).
    Simulation full = makeMixed(NeighborListMode::Full);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_FLOAT_EQ(half.atoms().forceX(i), full.atoms().forceX(i)) << "atom " << i;
        EXPECT_FLOAT_EQ(half.atoms().forceY(i), full.atoms().forceY(i));
        EXPECT_FLOAT_EQ(half.atoms().forceZ(i), full.atoms().forceZ(i));
    }
}
