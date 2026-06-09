#include <cmath>

#include <gtest/gtest.h>

#include "Engine/Simulation.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/ForceFields/BondForceField.h"
using namespace Lattice;

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
    sim.createWorld(glm::vec3{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(glm::vec3{40.0f, 40.0f, 40.0f}, 6);
    for (int i = 0; i < count; ++i) {
        sim.appendAtomFast(glm::vec3{15.0f + static_cast<float>(i) * 1.5f, 20.0f, 20.0f}, glm::vec3{0.0f, 0.0f, 0.0f},
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

namespace {

// Считает силы связей+углов на сцене 3 углерода / 2 связи переданным инстансом.
void computeCarbonChain(BondForceField& bff, Simulation& sim) {
    ASSERT_NE(Bond::CreateBond(sim.bonds(), 0, 1, sim.atoms()), nullptr);
    ASSERT_NE(Bond::CreateBond(sim.bonds(), 1, 2, sim.atoms()), nullptr);
    bff.compute(sim.atoms(), sim.bonds(), sim.neighborList(), /*allowBondFormation=*/false, /*dt=*/0.01f);
}

} // namespace

// BondForceField держит mutable scratch (degreeScratch_/neighborsScratch_ для
// углов, adjacencyScratch_ для dedup), переиспользуемый между вызовами compute()
// ради отказа от аллокаций каждый physics step. Проверяем, что переиспользование
// не оставляет stale-состояния: инстанс, уже посчитавший одну сцену, на следующей
// идентичной сцене даёт ПОБИТОВО те же силы, что свежий инстанс.
TEST(BondTest, BondForceFieldScratchReuseMatchesFresh) {
    BondForceField reused;
    Simulation warmup = makeSceneWithCarbons(3); // "загрязняем" scratch первой сценой
    computeCarbonChain(reused, warmup);

    Simulation viaReused = makeSceneWithCarbons(3);
    computeCarbonChain(reused, viaReused);

    BondForceField fresh;
    Simulation viaFresh = makeSceneWithCarbons(3);
    computeCarbonChain(fresh, viaFresh);

    for (size_t i = 0; i < 3; ++i) {
        EXPECT_FLOAT_EQ(viaReused.atoms().forceX(i), viaFresh.atoms().forceX(i)) << "atom " << i << " (scratch загрязнён)";
        EXPECT_FLOAT_EQ(viaReused.atoms().forceY(i), viaFresh.atoms().forceY(i)) << "atom " << i;
        EXPECT_FLOAT_EQ(viaReused.atoms().forceZ(i), viaFresh.atoms().forceZ(i)) << "atom " << i;
    }
}

// Путь формации связей по NL: compute(allowBondFormation=true) → formBonds →
// tryCreateBond → Bond::CreateBond(&adjacency). Тесты выше гоняют формацию ВЫКЛ
// либо Bond::CreateBond без adjacency; здесь покрываем включённую формацию и
// adjacency-ускоренный dup-check внутри неё.
TEST(BondTest, FormBondsViaNeighborListAndDedups) {
    Simulation sim = makeSceneWithCarbons(4); // 4 C, шаг 1.5 — в пределах формации
    sim.neighborList().build(sim.atoms(), sim.world());

    BondForceField bff;
    bff.compute(sim.atoms(), sim.bonds(), sim.neighborList(), /*allowBondFormation=*/true, /*dt=*/0.01f);
    const size_t formed = sim.bonds().size();
    EXPECT_GT(formed, 0u) << "по NL между близкими C должны сформироваться связи";

    // Повторная формация на той же сцене не должна дублировать (adjacency-dedup
    // в formBonds через Bond::CreateBond с непустым adjacency).
    bff.compute(sim.atoms(), sim.bonds(), sim.neighborList(), /*allowBondFormation=*/true, /*dt=*/0.01f);
    EXPECT_EQ(sim.bonds().size(), formed) << "adjacency-dedup: повторная формация не плодит дубли";
}

// Связь разрывается, когда атомы растянуты за kBondBreakDistance (3.0):
// BondForceField::compute в начале выкидывает bond по shouldBreak и через detach
// возвращает валентность обоим. Покрываем путь разрыва (раньше тесты только
// создавали связи и считали силы, разрыв не проверялся).
TEST(BondTest, BondBreaksWhenStretchedBeyondLimit) {
    Simulation sim = makeSceneWithCarbons(/*count=*/2);
    AtomStorage& atoms = sim.atoms();

    const uint8_t v0 = atoms.valenceCount(0);
    const uint8_t v1 = atoms.valenceCount(1);
    ASSERT_NE(Bond::CreateBond(sim.bonds(), 0, 1, atoms), nullptr);
    ASSERT_EQ(sim.bonds().size(), 1u);
    ASSERT_EQ(atoms.valenceCount(0), v0 - 1) << "valence списан при создании";

    // Растягиваем за предел разрыва: сдвигаем атом 1 на 5 (> 3.0) по x.
    atoms.posX(1) = atoms.posX(0) + 5.0f;

    BondForceField bondForceField;
    bondForceField.compute(atoms, sim.bonds(), sim.neighborList(), /*allowBondFormation=*/false, /*dt=*/0.01f);

    EXPECT_EQ(sim.bonds().size(), 0u) << "связь за пределом 3.0 должна разорваться";
    EXPECT_EQ(atoms.valenceCount(0), v0) << "разрыв через detach возвращает валентность";
    EXPECT_EQ(atoms.valenceCount(1), v1);
}

// formBonds пропускает пары дальше formationDistance = max(2.5, r0*1.35), даже
// если они соседи по NL. Покрываем дистанционный фильтр в tryCreateBond
// (раньше формация тестировалась только на заведомо близких атомах).
TEST(BondTest, FormBondsSkipsPairBeyondFormationDistance) {
    auto makePair = [](float separation) {
        Simulation sim;
        sim.createWorld(glm::vec3{40.0f, 40.0f, 40.0f});
        sim.setSizeBox(glm::vec3{40.0f, 40.0f, 40.0f}, 6);
        sim.appendAtomFast(glm::vec3{18.0f, 20.0f, 20.0f}, glm::vec3{0, 0, 0}, AtomData::Type::C);
        sim.appendAtomFast(glm::vec3{18.0f + separation, 20.0f, 20.0f}, glm::vec3{0, 0, 0}, AtomData::Type::C);
        sim.finalizeAtomBatch();
        sim.neighborList().build(sim.atoms(), sim.world());
        return sim;
    };

    // 5.5: в пределах NL listRadius (6), но далеко за formationDistance C-C.
    Simulation far = makePair(5.5f);
    BondForceField bffFar;
    bffFar.compute(far.atoms(), far.bonds(), far.neighborList(), /*allowBondFormation=*/true, /*dt=*/0.01f);
    EXPECT_EQ(far.bonds().size(), 0u) << "за formationDistance связь не образуется";

    // Контроль: на 1.5 (в пределах формации) связь образуется — фильтр не глушит всё.
    Simulation close = makePair(1.5f);
    BondForceField bffClose;
    bffClose.compute(close.atoms(), close.bonds(), close.neighborList(), /*allowBondFormation=*/true, /*dt=*/0.01f);
    EXPECT_GT(close.bonds().size(), 0u) << "в пределах formationDistance связь образуется";
}

// Bond::CreateBond отвергает связь, если у любого из атомов исчерпана валентность
// (valenceCount <= 0). Иначе атом получил бы больше связей, чем позволяет его
// химия. Покрываем проверку валентности (отдельную от duplicate-check).
TEST(BondTest, CreateBondRejectsWhenValenceExhausted) {
    Simulation sim = makeSceneWithCarbons(/*count=*/8);
    AtomStorage& atoms = sim.atoms();

    const int v0 = atoms.valenceCount(0);
    ASSERT_GE(v0, 1) << "C должен иметь валентность";
    ASSERT_LE(v0 + 1, 7) << "сцены из 8 атомов хватает на v0 соседей + 1 лишний";

    // Связываем атом 0 с v0 соседями — валентность атома 0 падает до нуля.
    for (int k = 1; k <= v0; ++k) {
        ASSERT_NE(Bond::CreateBond(sim.bonds(), 0, static_cast<size_t>(k), atoms), nullptr) << "bond 0-" << k;
    }
    ASSERT_EQ(static_cast<int>(atoms.valenceCount(0)), 0) << "валентность атома 0 исчерпана";
    const size_t bondsBefore = sim.bonds().size();

    // Следующая связь от исчерпанного атома 0 должна быть отвергнута.
    Bond* extra = Bond::CreateBond(sim.bonds(), 0, static_cast<size_t>(v0 + 1), atoms);
    EXPECT_EQ(extra, nullptr) << "при нулевой валентности связь не создаётся";
    EXPECT_EQ(sim.bonds().size(), bondsBefore) << "отвергнутая связь не добавлена в список";
}
