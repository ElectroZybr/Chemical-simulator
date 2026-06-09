// Golden (characterization) тесты: пиннят ТОЧНЫЙ побитовый вывод детерминированных
// функций на фиксированном входе. Любое будущее изменение вывода (намеренное или
// случайное) сразу ловится. Дополняет git-diff A/B (интегратор и формула LJ —
// нулевой/идентичный diff с дооптимизационным baseline fd28dc8, т.е. эти выводы
// уже доказанно не менялись; здесь фиксируем их конкретными значениями).
//
// Значения захвачены на текущем коде (clang/Windows). Несовпадение = либо
// изменение функции, либо смена платформы/компилятора (тогда обновить эталон
// осознанно, как и положено golden-тесту).

#include <cstdio>

#include <gtest/gtest.h>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
using namespace Lattice;

namespace {

// Захваченный эталон posX атома-угла 0 после 5 шагов (clang/Windows).
constexpr float kGoldenP0X = 20.0000381f;

Simulation makeFixed(NeighborListMode mode) {
    Simulation sim;
    sim.createWorld(glm::vec3{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(glm::vec3{40.0f, 40.0f, 40.0f}, 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setGravity(glm::vec3{0.0f, 0.0f, 0.0f});
    sim.setDt(0.01f);
    sim.setAccelDamping(1.0f);
    sim.neighborList().setMode(mode);
    return sim;
}

} // namespace

// Формула LJ-силы пары (LJForceField::pairInteraction) — git diff с baseline
// показывает арифметику неизменной. Пиннем силу на атоме 0 для пары H-H на
// фикс-расстоянии 1.3.
TEST(GoldenTest, LjPairForceHH) {
    Simulation sim = makeFixed(NeighborListMode::Half);
    sim.appendAtomFast(glm::vec3{20.0f, 20.0f, 20.0f}, glm::vec3{0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    sim.appendAtomFast(glm::vec3{21.3f, 20.0f, 20.0f}, glm::vec3{0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    sim.finalizeAtomBatch();
    sim.neighborList().build(sim.atoms(), sim.world());
    sim.forceField().computePairInteractions(sim.world());

    const float fx = sim.atoms().forceX(0);
    const float fy = sim.atoms().forceY(0);
    const float fz = sim.atoms().forceZ(0);
    std::printf("[ GOLDEN   ] LjPairForceHH f0 = (%.9g, %.9g, %.9g)\n", static_cast<double>(fx), static_cast<double>(fy),
                static_cast<double>(fz));
    // Newton-3: сила на атоме 1 строго противоположна.
    EXPECT_FLOAT_EQ(sim.atoms().forceX(1), -fx);
    EXPECT_EQ(fy, 0.0f);
    EXPECT_EQ(fz, 0.0f);
    // GOLDEN: точное значение формулы LJ-силы пары H-H на расстоянии 1.3.
    EXPECT_FLOAT_EQ(fx, -1714.42078f);
}

// LJ Half==Full: оба режима NL должны дать одинаковую суммарную силу на каждый
// атом (Half пишет силу обоим участникам пары, Full считает каждую сторону сам).
// Куб 2x2x2 шаг 3.0 — у каждого угла 6 соседей в пределах cutoff, реальная
// многососедняя суммация. Дополняет Coulomb-вариант (там 1 пара): здесь именно
// LJ-путь и накопление по нескольким соседям.
TEST(GoldenTest, LjHalfEqualsFull) {
    auto computeCube = [](NeighborListMode mode) {
        Simulation sim = makeFixed(mode);
        for (int z = 0; z < 2; ++z) {
            for (int y = 0; y < 2; ++y) {
                for (int x = 0; x < 2; ++x) {
                    sim.appendAtomFast(glm::vec3{20.0f + x * 3.0f, 20.0f + y * 3.0f, 20.0f + z * 3.0f}, glm::vec3{0, 0, 0},
                                       AtomData::Type::H);
                }
            }
        }
        sim.finalizeAtomBatch();
        sim.neighborList().build(sim.atoms(), sim.world());
        sim.forceField().computePairInteractions(sim.world());
        return sim;
    };

    Simulation half = computeCube(NeighborListMode::Half);
    Simulation full = computeCube(NeighborListMode::Full);
    ASSERT_EQ(half.atoms().size(), 8u);

    for (size_t i = 0; i < 8; ++i) {
        EXPECT_FLOAT_EQ(half.atoms().forceX(i), full.atoms().forceX(i)) << "atom " << i << " fx";
        EXPECT_FLOAT_EQ(half.atoms().forceY(i), full.atoms().forceY(i)) << "atom " << i << " fy";
        EXPECT_FLOAT_EQ(half.atoms().forceZ(i), full.atoms().forceZ(i)) << "atom " << i << " fz";
    }
}

// Полный детерминированный CPU-шаг (формула + цикл + cutoff + интегратор) на
// фиксированной 8-атомной сцене, Half/serial. Пиннем позиции после 1 update().
TEST(GoldenTest, SingleStepLattice) {
    Simulation sim = makeFixed(NeighborListMode::Half);
    // Кубик 2x2x2, шаг 3.0 (в пределах cutoff, стабильно — не взрывается).
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                sim.appendAtomFast(glm::vec3{20.0f + x * 3.0f, 20.0f + y * 3.0f, 20.0f + z * 3.0f}, glm::vec3{0.0f, 0.0f, 0.0f},
                                   AtomData::Type::H);
            }
        }
    }
    sim.finalizeAtomBatch();
    for (int s = 0; s < 5; ++s) {
        sim.update(); // 5 шагов — позиции реально смещаются (predict использует силу пред. шага)
    }

    const AtomStorage& a = sim.atoms();
    std::printf("[ GOLDEN   ] SingleStepLattice after 5 steps:\n");
    for (size_t i = 0; i < a.size(); ++i) {
        std::printf("[ GOLDEN   ]   p%zu = (%.9g, %.9g, %.9g)\n", i, static_cast<double>(a.posX(i)), static_cast<double>(a.posY(i)),
                    static_cast<double>(a.posZ(i)));
    }
    // Симметрия кубика под инверсией через центр 21.5: атом 0 и атом 7 смещаются
    // зеркально. Sanity без хардкода значений.
    EXPECT_NEAR(static_cast<double>(a.posX(0)) - 20.0, 23.0 - static_cast<double>(a.posX(7)), 1e-5);
    // GOLDEN: точные позиции угла впишем после захвата.
    EXPECT_FLOAT_EQ(a.posX(0), kGoldenP0X);
}
