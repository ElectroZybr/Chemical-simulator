// Тесты физической корректности оптимизированного CPU-пути. Цель НЕ "как до
// оптимизаций" (это недостижимо: МД хаотична, а часть правок намеренно меняла
// поведение — напр. cutoff-фильтрация сил), а инварианты, которые ловят РЕАЛЬНЫЕ
// баги: потерянные/несимметричные силы и недетерминизм параллельного прохода.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
using namespace Lattice;

namespace {

// Кубическая решётка n атомов H, шаг spacing (стабильный — в пределах cutoff,
// как в бенчах), глубоко внутри box (margin от стен), с малыми seeded-скоростями.
// LJ-only, без гравитации/связей, accelDamping=1 → консервативная замкнутая
// система. setMode фиксирует режим NL (Half=serial, Full+>=5000=TBB parallel).
Simulation makeBulk(uint32_t n, float box, float spacing, float velScale, uint32_t seed, NeighborListMode mode) {
    Simulation sim;
    sim.createWorld(Vec3f{box, box, box});
    sim.setSizeBox(Vec3f{box, box, box}, 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
    sim.setDt(0.005f);
    sim.setAccelDamping(1.0f); // без демпфирования — импульс/энергия не диссипируют
    sim.neighborList().setMode(mode);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> vel(-velScale, velScale);
    const int side = static_cast<int>(std::cbrt(static_cast<double>(n))) + 1;
    const float margin = 12.0f;
    uint32_t placed = 0;
    for (int z = 0; z < side && placed < n; ++z) {
        for (int y = 0; y < side && placed < n; ++y) {
            for (int x = 0; x < side && placed < n; ++x, ++placed) {
                sim.appendAtomFast(Vec3f{margin + x * spacing, margin + y * spacing, margin + z * spacing},
                                   Vec3f{vel(rng), vel(rng), vel(rng)}, AtomData::Type::H, /*fixed=*/false);
            }
        }
    }
    sim.finalizeAtomBatch();

    // Обнуляем суммарный импульс: вычитаем средневзвешенную скорость ц.м.
    AtomStorage& a = sim.atoms();
    double px = 0, py = 0, pz = 0, mtot = 0;
    for (size_t i = 0; i < a.mobileCount(); ++i) {
        const double m = 1.0 / a.invMass(i);
        px += m * a.velX(i);
        py += m * a.velY(i);
        pz += m * a.velZ(i);
        mtot += m;
    }
    if (mtot > 0) {
        const float vx = static_cast<float>(px / mtot), vy = static_cast<float>(py / mtot), vz = static_cast<float>(pz / mtot);
        for (size_t i = 0; i < a.mobileCount(); ++i) {
            a.velX(i) -= vx;
            a.velY(i) -= vy;
            a.velZ(i) -= vz;
        }
    }
    return sim;
}

struct Momentum {
    double mag;   // |P|
    double scale; // sum |m_i v_i| — масштаб без сокращения
};

Momentum totalMomentum(const AtomStorage& a) {
    double px = 0, py = 0, pz = 0, scale = 0;
    for (size_t i = 0; i < a.mobileCount(); ++i) {
        const double m = 1.0 / a.invMass(i);
        const double vx = a.velX(i), vy = a.velY(i), vz = a.velZ(i);
        px += m * vx;
        py += m * vy;
        pz += m * vz;
        scale += m * std::sqrt(vx * vx + vy * vy + vz * vz);
    }
    return {std::sqrt(px * px + py * py + pz * pz), scale};
}

} // namespace

// Парные LJ-силы подчиняются Ньютону-3, поэтому суммарный импульс замкнутой
// системы сохраняется. Стартуем с P=0 -> после шагов |P| должно остаться в
// fp-шуме (<< масштаба Σ|m v|). Реальная асимметрия (двойной счёт в Full,
// потерянная сторона пары, баг параллельного прохода) увела бы |P| на O(force).
TEST(ConservationTest, MomentumConservedFullParallel) {
    Simulation sim = makeBulk(/*n=*/6000, /*box=*/160.0f, /*spacing=*/3.0f, /*velScale=*/0.3f, /*seed=*/1, NeighborListMode::Full);
    ASSERT_GE(sim.atoms().mobileCount(), 5000u) << "нужно >=5000 mobile для TBB parallel-пути";

    for (int s = 0; s < 40; ++s) {
        sim.update();
    }
    const Momentum p = totalMomentum(sim.atoms());
    std::printf("[ MOMENTUM ] Full/parallel N=%zu |P|=%.4e scale=%.4e ratio=%.2e\n", sim.atoms().size(), p.mag, p.scale,
                p.scale > 0 ? p.mag / p.scale : 0.0);
    // |P| должно быть пренебрежимо мало относительно масштаба движения.
    EXPECT_LT(p.mag, 1e-3 * p.scale) << "суммарный импульс дрейфует — асимметрия параллельного force-loop";
}

TEST(ConservationTest, MomentumConservedHalfSerial) {
    Simulation sim = makeBulk(/*n=*/512, /*box=*/60.0f, /*spacing=*/3.0f, /*velScale=*/0.3f, /*seed=*/2, NeighborListMode::Half);
    for (int s = 0; s < 40; ++s) {
        sim.update();
    }
    const Momentum p = totalMomentum(sim.atoms());
    std::printf("[ MOMENTUM ] Half/serial N=%zu |P|=%.4e scale=%.4e ratio=%.2e\n", sim.atoms().size(), p.mag, p.scale,
                p.scale > 0 ? p.mag / p.scale : 0.0);
    EXPECT_LT(p.mag, 1e-3 * p.scale) << "суммарный импульс дрейфует — асимметрия serial force-loop / Half NL";
}

// Полная энергия LJ-системы (accelDamping=1, атомы внутри box — стены не
// диссипируют). ВАЖНО: усечённый по cutoff LJ (сила обрезана, потенциал не
// сдвинут — следствие cutoff-фильтрации сил) не сохраняет энергию ИДЕАЛЬНО:
// пары, пересекающие cutoff, дают малые ступеньки. Поэтому проверяем не строгое
// сохранение, а главное: (1) энергия КОНЕЧНА на каждом шаге — прямой страж от
// "атомы исчезают" (исчезновение = позиция ушла в NaN/inf от разлёта);
// (2) энергия не РАЗЛЕТАЕТСЯ — дрейф мал относительно масштаба энергии. Реальный
// взрыв сил или ошибка интегратора -> энергия уходит на порядки -> rel >> 1.
TEST(ConservationTest, EnergyBoundedNoBlowup) {
    Simulation sim = makeBulk(/*n=*/512, /*box=*/70.0f, /*spacing=*/3.4f, /*velScale=*/0.25f, /*seed=*/11, NeighborListMode::Full);
    sim.setDt(0.003f);

    sim.update(); // первый шаг — заполняет силы/PE
    const double ke0 = sim.averageKineticEnergyEv();
    const double pe0 = sim.averagePotentialEnergyEv();
    const double e0 = ke0 + pe0;
    const double scale = std::abs(ke0) + std::abs(pe0); // масштаб энергии (не близок к нулю)
    ASSERT_TRUE(std::isfinite(e0)) << "стартовая энергия не конечна";
    ASSERT_GT(scale, 1e-6) << "система холодная — масштаб энергии вырожден";

    double maxAbsDrift = 0.0;
    for (int s = 0; s < 200; ++s) {
        sim.update();
        const double e = sim.averageKineticEnergyEv() + sim.averagePotentialEnergyEv();
        ASSERT_TRUE(std::isfinite(e)) << "энергия стала NaN/inf на шаге " << s << " — атомы разлетелись/исчезли";
        maxAbsDrift = std::max(maxAbsDrift, std::abs(e - e0));
    }
    const double rel = maxAbsDrift / scale;
    std::printf("[ ENERGY   ] N=512 E0=%.4e scale=%.4e maxDrift=%.4e (%.1f%% scale) over 200 steps\n", e0, scale, maxAbsDrift,
                100.0 * rel);
    EXPECT_LT(rel, 0.25) << "энергия уходит > 25%% масштаба — разлёт/взрыв (баг сил или интегратора)";
}

// TBB-параллельный force-loop пишет силы по-атомно (без общей редукции), поэтому
// обязан быть детерминированным: один seed + те же шаги -> бит-в-бит те же
// позиции. Падение = гонка или зависимость от порядка планировщика.
TEST(ConservationTest, ParallelStepIsDeterministic) {
    auto run = []() {
        Simulation sim = makeBulk(/*n=*/6000, /*box=*/160.0f, /*spacing=*/3.0f, /*velScale=*/0.3f, /*seed=*/7, NeighborListMode::Full);
        for (int s = 0; s < 15; ++s) {
            sim.update();
        }
        const AtomStorage& a = sim.atoms();
        std::vector<float> pos;
        pos.reserve(a.size() * 3);
        for (size_t i = 0; i < a.size(); ++i) {
            pos.push_back(a.posX(i));
            pos.push_back(a.posY(i));
            pos.push_back(a.posZ(i));
        }
        return pos;
    };

    const std::vector<float> r1 = run();
    const std::vector<float> r2 = run();
    ASSERT_EQ(r1.size(), r2.size());
    size_t mismatches = 0;
    for (size_t i = 0; i < r1.size(); ++i) {
        if (r1[i] != r2[i]) {
            ++mismatches;
        }
    }
    std::printf("[ DETERM   ] parallel run mismatches=%zu / %zu floats\n", mismatches, r1.size());
    EXPECT_EQ(mismatches, 0u) << "недетерминизм параллельного force-loop (гонка / порядок планировщика)";
}
