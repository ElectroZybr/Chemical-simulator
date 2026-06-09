#include <cmath>

#include <gtest/gtest.h>

#include "Engine/Simulation.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
using namespace Lattice;

namespace {

// Hybrid tolerance: abs(a-b) <= max(absTol, relTol * max(|a|, |b|)).
// Заметка: после симметричной записи "forceA -= pairF; forceB += pairF" значение pairF
// одно и то же, так что сумма forceA+forceB должна быть бит-равна нулю при
// одинарном вычислении пары. 1e-12 — sanity на компилятор, а не на численный шум.
::testing::AssertionResult FpNear(float a, float b, float absTol, float relTol, const char* lhs, const char* rhs) {
    const float diff = std::abs(a - b);
    const float scale = std::max(std::abs(a), std::abs(b));
    const float tol = std::max(absTol, relTol * scale);
    if (diff <= tol) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << lhs << " = " << a << " vs " << rhs << " = " << b
                                         << "; |diff| = " << diff << " > tol = " << tol;
}

#define EXPECT_FP_NEAR(a, b, absTol, relTol) EXPECT_TRUE(FpNear((a), (b), (absTol), (relTol), #a, #b))

Simulation makeTwoAtomScene(float r, bool secondFixed) {
    Simulation sim;
    sim.createWorld(glm::vec3{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(glm::vec3{40.0f, 40.0f, 40.0f}, 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);

    sim.appendAtomFast(glm::vec3{20.0f, 20.0f, 20.0f},       glm::vec3{0.0f, 0.0f, 0.0f}, AtomData::Type::H, /*fixed=*/false);
    sim.appendAtomFast(glm::vec3{20.0f, 20.0f, 20.0f + r},   glm::vec3{0.0f, 0.0f, 0.0f}, AtomData::Type::H, /*fixed=*/secondFixed);
    sim.finalizeAtomBatch();

    sim.neighborList().build(sim.atoms(), sim.world());
    return sim;
}

} // namespace

// Pair-сила в LJ записывается через "forceA -= pairF; forceB += pairF" в один проход
// внутри pairInteraction. Newton 3rd law держится бит-равно при одном вычислении.
TEST(ForceFieldTest, PairNewton3) {
    Simulation sim = makeTwoAtomScene(/*r=*/2.5f, /*secondFixed=*/false);
    sim.forceField().computePairInteractions(sim.world());

    const AtomStorage& atoms = sim.atoms();
    EXPECT_FP_NEAR(atoms.forceX(0), -atoms.forceX(1), 1e-12f, 1e-12f);
    EXPECT_FP_NEAR(atoms.forceY(0), -atoms.forceY(1), 1e-12f, 1e-12f);
    EXPECT_FP_NEAR(atoms.forceZ(0), -atoms.forceZ(1), 1e-12f, 1e-12f);

    // sanity: сила вообще ненулевая (sticky чтобы Newton-3 не прошёл тривиально).
    const float fmag = std::sqrt(atoms.forceX(0) * atoms.forceX(0) + atoms.forceY(0) * atoms.forceY(0) +
                                 atoms.forceZ(0) * atoms.forceZ(0));
    EXPECT_GT(fmag, 0.0f);
}

// Атомы на дистанции 5.5 (между cutoff=5 и listRadius=6) попадают в NL как
// потенциальные соседи (NL хранит пары до listRadius), но force loop должен
// фильтровать их по физическому cutoff и не давать LJ-силу.
TEST(ForceFieldTest, StrictCutoffFiltersBeyondCutoff) {
    Simulation sim = makeTwoAtomScene(/*r=*/5.5f, /*secondFixed=*/false);
    sim.forceField().computePairInteractions(sim.world());

    const AtomStorage& atoms = sim.atoms();
    EXPECT_FLOAT_EQ(atoms.forceX(0), 0.0f);
    EXPECT_FLOAT_EQ(atoms.forceY(0), 0.0f);
    EXPECT_FLOAT_EQ(atoms.forceZ(0), 0.0f);
}

// Решение по Bug 6: атомы с fixed=true в текущей реализации — декоративные,
// они не участвуют в pair-силах (mobile-first invariant + half-NL + force loop
// до mobileCount исключают mobile-fixed пары). Этот тест защищает текущее
// поведение от случайной "починки" в сторону anchor/pinned-семантики.
TEST(ForceFieldTest, MobileFixedPairDoesNotInteract) {
    Simulation sim = makeTwoAtomScene(/*r=*/3.0f, /*secondFixed=*/true);
    ASSERT_EQ(sim.atoms().mobileCount(), 1u) << "scene должна иметь ровно 1 mobile атом";

    sim.forceField().computePairInteractions(sim.world());

    const AtomStorage& atoms = sim.atoms();
    EXPECT_EQ(atoms.forceX(0), 0.0f);
    EXPECT_EQ(atoms.forceY(0), 0.0f);
    EXPECT_EQ(atoms.forceZ(0), 0.0f);
}
