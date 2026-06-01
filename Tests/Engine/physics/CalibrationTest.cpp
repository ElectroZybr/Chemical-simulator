#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
using namespace Lattice;

namespace {

struct Pos3 {
    float x, y, z;
};

std::vector<Pos3> samplePositions(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> coord(8.0f, 32.0f);
    std::vector<Pos3> positions(n);
    for (auto& p : positions) {
        p.x = coord(rng);
        p.y = coord(rng);
        p.z = coord(rng);
    }
    return positions;
}

Simulation makeSceneFromPositions(const std::vector<Pos3>& positions, bool reversed) {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);

    if (reversed) {
        for (auto it = positions.rbegin(); it != positions.rend(); ++it) {
            sim.appendAtomFast(Vec3f{it->x, it->y, it->z}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::H, false);
        }
    }
    else {
        for (const auto& p : positions) {
            sim.appendAtomFast(Vec3f{p.x, p.y, p.z}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::H, false);
        }
    }
    sim.finalizeAtomBatch();
    sim.neighborList().build(sim.atoms(), sim.world());
    sim.forceField().computePairInteractions(sim.world());
    return sim;
}

struct ForceDelta {
    float maxAbs = 0.0f;
    float maxRel = 0.0f;
};

ForceDelta compareByPosition(const Simulation& a, const Simulation& b, const std::vector<Pos3>& positions) {
    ForceDelta delta;
    const AtomStorage& atomsA = a.atoms();
    const AtomStorage& atomsB = b.atoms();
    const size_t n = positions.size();

    for (size_t k = 0; k < n; ++k) {
        // a — порядок прямой, b — обратный, индексы по позиции:
        const size_t indexA = k;
        const size_t indexB = n - 1 - k;

        const float fxa = atomsA.forceX(indexA), fxb = atomsB.forceX(indexB);
        const float fya = atomsA.forceY(indexA), fyb = atomsB.forceY(indexB);
        const float fza = atomsA.forceZ(indexA), fzb = atomsB.forceZ(indexB);

        for (auto [a_val, b_val] : {std::pair{fxa, fxb}, {fya, fyb}, {fza, fzb}}) {
            const float diff = std::abs(a_val - b_val);
            const float scale = std::max(std::abs(a_val), std::abs(b_val));
            delta.maxAbs = std::max(delta.maxAbs, diff);
            if (scale > 1e-9f) {
                delta.maxRel = std::max(delta.maxRel, diff / scale);
            }
        }
    }
    return delta;
}

} // namespace

// CalibrationTest НЕ имеет EXPECT — это измерение, не assertion. Цифры идут в
// gtest_output: D5 (TBB parallel) потом возьмёт max(floor, 10x этих чисел) как
// нижнюю границу tolerance, и зафиксирует это в commit message.
//
// Подход: одинаковые случайные позиции → две сцены, в одной insertion прямой,
// в другой обратный. NL построится с разной индексацией, force loop просуммирует
// LJ-вклады в другом порядке, и любая fp-неассоциативность проявится как
// разница сил на одной и той же физической позиции.
//
// Два одинаковых run'а измеряли бы детерминизм, не noise. Разные compile flags
// измеряли бы compiler drift. Прямой/обратный insertion в одном binary — это
// именно та модель reorder'а, которую D5 reduction внесёт в parallel-сумму.
TEST(CalibrationTest, PairForceFpNoiseUnderIndexReorder) {
    constexpr size_t kN = 256;
    constexpr uint32_t kSeed = 12345;
    const auto positions = samplePositions(kN, kSeed);

    Simulation forward = makeSceneFromPositions(positions, /*reversed=*/false);
    Simulation reversed = makeSceneFromPositions(positions, /*reversed=*/true);

    const ForceDelta delta = compareByPosition(forward, reversed, positions);

    std::printf("[ CALIB    ] fp noise floor for pair-LJ index reorder, N = %zu:\n", kN);
    std::printf("[ CALIB    ]   max |a - b|         = %.6e\n", static_cast<double>(delta.maxAbs));
    std::printf("[ CALIB    ]   max |a - b| / scale = %.6e\n", static_cast<double>(delta.maxRel));
    std::printf("[ CALIB    ] D5 tolerance recipe: max(1e-9, 10x calibration max)\n");
    std::printf("[ CALIB    ]   suggested D5 absTol = %.6e\n", std::max(1e-9, 10.0 * delta.maxAbs));
    std::printf("[ CALIB    ]   suggested D5 relTol = %.6e\n", std::max(1e-10, 10.0 * delta.maxRel));

    // sanity: что-то отлично от нуля должно быть, иначе reorder не сработал.
    EXPECT_GT(delta.maxAbs + delta.maxRel, 0.0f)
        << "оба пути дали бит-равные результаты — index reorder не повлиял на сумму";
}
