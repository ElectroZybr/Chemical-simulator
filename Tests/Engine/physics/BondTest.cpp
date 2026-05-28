#include <cmath>

#include <gtest/gtest.h>

#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/ForceFields/BondForceField.h"

namespace {

::testing::AssertionResult FpAbs(float a, float b, float tol, const char* lhs, const char* rhs) {
    const float diff = std::abs(a - b);
    if (diff <= tol) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << lhs << " = " << a << " vs " << rhs << " = " << b
                                         << "; |diff| = " << diff << " > tol = " << tol;
}

#define EXPECT_FP_ABS(a, b, tol) EXPECT_TRUE(FpAbs((a), (b), (tol), #a, #b))

Simulation makeSceneWithCarbons(int count) {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, 6);
    for (int i = 0; i < count; ++i) {
        sim.appendAtomFast(Vec3f{15.0f + static_cast<float>(i) * 1.5f, 20.0f, 20.0f}, Vec3f{0.0f, 0.0f, 0.0f},
                           AtomData::Type::C, /*fixed=*/false);
    }
    sim.finalizeAtomBatch();
    return sim;
}

} // namespace

// Bond::CreateBond уменьшает valenceCount обоих атомов на 1 при успехе.
// На этом инварианте держится duplicate check и общая bond-формация —
// если valence не списан, повторный bond пройдёт.
TEST(BondTest, CreateBondDeducesValence) {
    Simulation sim = makeSceneWithCarbons(/*count=*/2);
    AtomStorage& atoms = sim.atoms();

    const uint8_t v0Before = atoms.valenceCount(0);
    const uint8_t v1Before = atoms.valenceCount(1);
    ASSERT_GE(v0Before, 1u) << "C должен иметь valence >= 1";

    Bond* bond = Bond::CreateBond(sim.bonds(), 0, 1, atoms);
    ASSERT_NE(bond, nullptr);

    EXPECT_EQ(atoms.valenceCount(0), v0Before - 1);
    EXPECT_EQ(atoms.valenceCount(1), v1Before - 1);
    EXPECT_EQ(sim.bonds().size(), 1u);
}

// Дубликатный bond между уже связанной парой должен быть отвергнут даже когда у
// атомов остался свободный valence. Это защищает учётность графа связей.
TEST(BondTest, CreateBondRejectsDuplicate) {
    Simulation sim = makeSceneWithCarbons(/*count=*/2);
    AtomStorage& atoms = sim.atoms();

    Bond* first = Bond::CreateBond(sim.bonds(), 0, 1, atoms);
    ASSERT_NE(first, nullptr);

    const uint8_t v0After = atoms.valenceCount(0);
    const uint8_t v1After = atoms.valenceCount(1);
    ASSERT_GE(v0After, 1u) << "у C должен остаться valence для теоретически возможного дубля";

    Bond* duplicate = Bond::CreateBond(sim.bonds(), 0, 1, atoms);
    EXPECT_EQ(duplicate, nullptr);
    EXPECT_EQ(sim.bonds().size(), 1u);

    // valence не должен уменьшиться повторно — иначе дубль "прошёл наполовину".
    EXPECT_EQ(atoms.valenceCount(0), v0After);
    EXPECT_EQ(atoms.valenceCount(1), v1After);

    // Обратный порядок (b, a) тоже должен быть отвергнут как тот же edge.
    Bond* reversed = Bond::CreateBond(sim.bonds(), 1, 0, atoms);
    EXPECT_EQ(reversed, nullptr);
    EXPECT_EQ(sim.bonds().size(), 1u);
}

// BondForceField::compute суммарно даёт forceBond (Morse) + applyAngleForces.
// Обе суб-силы Newton-3 conserved, поэтому сумма сил на изолированную систему
// должна быть ~0 — Newton-3 в глобальной форме.
// Tolerance 1e-10 (double): угловой расчёт использует double + sqrt + acos +
// деления, шум накапливается сильнее, чем в pair-LJ.
TEST(BondTest, BondedSystemNewton3) {
    Simulation sim = makeSceneWithCarbons(/*count=*/3);
    AtomStorage& atoms = sim.atoms();

    // 2 bond: 0-1 и 1-2. Центр угла — атом 1.
    ASSERT_NE(Bond::CreateBond(sim.bonds(), 0, 1, atoms), nullptr);
    ASSERT_NE(Bond::CreateBond(sim.bonds(), 1, 2, atoms), nullptr);
    ASSERT_EQ(sim.bonds().size(), 2u);

    // forces инициализированы нулями (AtomStorage::init / addAtom).
    // allowBondFormation=false — не трогаем NL, не лазим в formBonds.
    BondForceField bondForceField;
    bondForceField.compute(atoms, sim.bonds(), sim.neighborList(), /*allowBondFormation=*/false,
                           /*dt=*/0.01f);

    const float sumFx = atoms.forceX(0) + atoms.forceX(1) + atoms.forceX(2);
    const float sumFy = atoms.forceY(0) + atoms.forceY(1) + atoms.forceY(2);
    const float sumFz = atoms.forceZ(0) + atoms.forceZ(1) + atoms.forceZ(2);

    constexpr float kAbsTol = 1e-10f;
    EXPECT_FP_ABS(sumFx, 0.0f, kAbsTol);
    EXPECT_FP_ABS(sumFy, 0.0f, kAbsTol);
    EXPECT_FP_ABS(sumFz, 0.0f, kAbsTol);
}
