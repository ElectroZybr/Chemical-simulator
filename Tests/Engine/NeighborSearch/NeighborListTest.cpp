#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <utility>

#include <gtest/gtest.h>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"

namespace {

using PairSet = std::set<std::pair<uint32_t, uint32_t>>;

PairSet collectNlPairs(const NeighborList& nl, uint32_t mobileCount) {
    PairSet pairs;
    const auto& offsets = nl.offsets();
    const auto& neighbors = nl.neighbors();
    for (uint32_t i = 0; i < mobileCount; ++i) {
        for (uint32_t p = offsets[i]; p < offsets[i + 1]; ++p) {
            const uint32_t j = neighbors[p];
            // NL хранит half-list (j < i), приводим к каноническому виду (lo, hi).
            const uint32_t lo = std::min(i, j);
            const uint32_t hi = std::max(i, j);
            pairs.emplace(lo, hi);
        }
    }
    return pairs;
}

PairSet bruteForcePairsWithin(const AtomStorage& atoms, float radius) {
    PairSet pairs;
    const float r2 = radius * radius;
    const size_t n = atoms.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const float dx = atoms.posX(j) - atoms.posX(i);
            const float dy = atoms.posY(j) - atoms.posY(i);
            const float dz = atoms.posZ(j) - atoms.posZ(i);
            if (dx * dx + dy * dy + dz * dz <= r2) {
                pairs.emplace(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
            }
        }
    }
    return pairs;
}

Simulation makeRandomScene(uint32_t n, uint32_t seed) {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    // cellSize=6 совпадает с дефолтом WorldState, listRadius = cutoff(5)+skin(1) = 6.
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, 6);

    std::mt19937 rng(seed);
    // Атомы в окне [6, 34] чтобы попасть в interior cells (ghost layer = 1, cellSize = 6).
    std::uniform_real_distribution<float> coord(6.0f, 34.0f);

    for (uint32_t i = 0; i < n; ++i) {
        sim.appendAtomFast(Vec3f{coord(rng), coord(rng), coord(rng)}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    }
    sim.finalizeAtomBatch();
    return sim;
}

} // namespace

// listRadius = cutoff(5) + skin(1) = 6. NL по контракту хранит все пары на расстоянии
// <= listRadius (через 27-cell stencil + filter). Brute-force с тем же радиусом должен
// дать идентичное множество пар после приведения к каноническому виду (lo, hi).
TEST(NeighborListTest, MatchesBruteForce) {
    Simulation sim = makeRandomScene(/*n=*/64, /*seed=*/42);
    NeighborList& nl = sim.neighborList();
    nl.build(sim.atoms(), sim.world());

    const PairSet nlPairs = collectNlPairs(nl, static_cast<uint32_t>(sim.atoms().mobileCount()));
    const PairSet bfPairs = bruteForcePairsWithin(sim.atoms(), nl.listRadius());

    EXPECT_EQ(nlPairs, bfPairs);
}

// needsRebuild() сигнализирует true когда любой mobile-атом сместился более чем
// на 0.5*skin от reference position. Тригер должен сработать после явного сдвига
// одного атома на skin (= 1.0 при default-параметрах).
TEST(NeighborListTest, RebuildTriggersOnDisplacement) {
    Simulation sim = makeRandomScene(/*n=*/8, /*seed=*/7);
    NeighborList& nl = sim.neighborList();
    nl.build(sim.atoms(), sim.world());

    ASSERT_FALSE(nl.needsRebuild(sim.atoms())) << "сразу после build NL должен быть валидным";

    // skin = 1.0 (default). Сдвиг = skin > 0.5*skin = 0.5 — должен тригернуть rebuild.
    AtomStorage& atoms = sim.atoms();
    atoms.posX(0) += nl.skin();

    EXPECT_TRUE(nl.needsRebuild(atoms));
}
