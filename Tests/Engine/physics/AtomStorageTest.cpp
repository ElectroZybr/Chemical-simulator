#include <gtest/gtest.h>

#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
using namespace Lattice;

namespace {

Simulation makeEmptySim() {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, 6);
    return sim;
}

} // namespace

// AtomStorage держит инвариант: первые mobileCount() слотов — подвижные,
// остальные — фиксированные. addAtom(fixed=true) кладёт в конец, addAtom(fixed=false)
// делает swap с границей mobileCount. removeAtom любого типа должен этот инвариант
// сохранить. Force loop и integrator опираются на это: они итерируют [0, mobileCount).
TEST(AtomStorageTest, MobileFirstAfterAddMixed) {
    Simulation sim = makeEmptySim();

    for (int i = 0; i < 10; ++i) {
        sim.appendAtomFast(Vec3f{10.0f + static_cast<float>(i), 10.0f, 10.0f}, Vec3f{0.0f, 0.0f, 0.0f},
                           AtomData::Type::H, /*fixed=*/false);
    }
    for (int i = 0; i < 5; ++i) {
        sim.appendAtomFast(Vec3f{10.0f + static_cast<float>(i), 20.0f, 10.0f}, Vec3f{0.0f, 0.0f, 0.0f},
                           AtomData::Type::H, /*fixed=*/true);
    }
    sim.finalizeAtomBatch();

    ASSERT_EQ(sim.atoms().size(), 15u);
    ASSERT_EQ(sim.atoms().mobileCount(), 10u);

    // Удаляем mobile в середине — mobileCount должен уменьшиться,
    // оставшиеся mobile в [0, 9), fixed в [9, 14).
    sim.removeAtom(3);

    EXPECT_EQ(sim.atoms().size(), 14u);
    EXPECT_EQ(sim.atoms().mobileCount(), 9u);

    const AtomStorage& atoms = sim.atoms();
    for (size_t i = 0; i < atoms.mobileCount(); ++i) {
        EXPECT_FALSE(atoms.isAtomFixed(i)) << "atom #" << i << " должен быть mobile";
    }
    for (size_t i = atoms.mobileCount(); i < atoms.size(); ++i) {
        EXPECT_TRUE(atoms.isAtomFixed(i)) << "atom #" << i << " должен быть fixed";
    }

    // И удаление fixed-атома: размер вниз, mobileCount без изменений.
    sim.removeAtom(atoms.mobileCount()); // первый fixed

    EXPECT_EQ(atoms.size(), 13u);
    EXPECT_EQ(atoms.mobileCount(), 9u);
}

// floatData_ растёт 1.5x+1 при превышении capacity. Реальная цель — после многих
// addAtom не теряются данные ранее добавленных атомов (rebind указателей на новый
// std::vector делает старые сырые pointer'ы dangling, если что-то не учёл).
TEST(AtomStorageTest, GrowthPreservesEarlierAtoms) {
    Simulation sim = makeEmptySim();

    // World::World уже резервирует 250k, поэтому 300k гарантирует хотя бы одну
    // реальную перестройку floatData_.
    constexpr size_t kN = 300'000;
    sim.reserveAtoms(kN);

    for (size_t i = 0; i < kN; ++i) {
        const float fi = static_cast<float>(i);
        sim.appendAtomFast(Vec3f{fi, fi * 2.0f, fi * 3.0f}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::H,
                           /*fixed=*/false);
    }

    const AtomStorage& atoms = sim.atoms();
    ASSERT_EQ(atoms.size(), kN);
    ASSERT_EQ(atoms.mobileCount(), kN);

    // Spot check: первый, середина, последний.
    EXPECT_FLOAT_EQ(atoms.posX(0), 0.0f);
    EXPECT_FLOAT_EQ(atoms.posY(0), 0.0f);
    EXPECT_FLOAT_EQ(atoms.posZ(0), 0.0f);

    const size_t mid = kN / 2;
    EXPECT_FLOAT_EQ(atoms.posX(mid), static_cast<float>(mid));
    EXPECT_FLOAT_EQ(atoms.posY(mid), static_cast<float>(mid) * 2.0f);
    EXPECT_FLOAT_EQ(atoms.posZ(mid), static_cast<float>(mid) * 3.0f);

    EXPECT_FLOAT_EQ(atoms.posX(kN - 1), static_cast<float>(kN - 1));
    EXPECT_FLOAT_EQ(atoms.posY(kN - 1), static_cast<float>(kN - 1) * 2.0f);
    EXPECT_FLOAT_EQ(atoms.posZ(kN - 1), static_cast<float>(kN - 1) * 3.0f);
}
