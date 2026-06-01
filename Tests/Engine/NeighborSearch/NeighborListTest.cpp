#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
using namespace Lattice;

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

// Расширенная проверка пар vs брутфорс: множество плотностей, размеров, seed'ов
// и обоих режимов NL (Half/Full). Это страховка от "потерянных пар" — если
// grid/27-cell stencil/режим теряет соседа, атом не получит силу и улетит "сам
// по себе". Half и Full после канонизации (lo,hi) дают одно множество = брутфорс.
TEST(NeighborListTest, MatchesBruteForceSweep) {
    struct Cfg {
        uint32_t n;
        uint32_t seed;
        NeighborListMode mode;
        const char* tag;
    };
    const Cfg cfgs[] = {
        {16, 1, NeighborListMode::Half, "sparse/half"},   {16, 1, NeighborListMode::Full, "sparse/full"},
        {128, 2, NeighborListMode::Half, "mid/half"},     {128, 2, NeighborListMode::Full, "mid/full"},
        {512, 3, NeighborListMode::Half, "dense/half"},   {512, 3, NeighborListMode::Full, "dense/full"},
        {512, 99, NeighborListMode::Full, "dense/seed99"}, {1000, 7, NeighborListMode::Full, "verydense/full"},
    };
    for (const auto& c : cfgs) {
        Simulation sim = makeRandomScene(c.n, c.seed);
        sim.neighborList().setMode(c.mode);
        sim.neighborList().build(sim.atoms(), sim.world());

        const PairSet nlPairs = collectNlPairs(sim.neighborList(), static_cast<uint32_t>(sim.atoms().mobileCount()));
        const PairSet bfPairs = bruteForcePairsWithin(sim.atoms(), sim.neighborList().listRadius());

        EXPECT_EQ(nlPairs, bfPairs) << "config " << c.tag << " n=" << c.n;
    }
}

// setAutoMode выбирает режим NL на каждом build по mobileCount: ниже порога —
// Half (дешевле на малых сценах), на/выше — Full (parallel-выгода). Покрываем
// сам выбор (Sweep использует явный setMode и авто-логику не трогает).
TEST(NeighborListTest, AutoModeSelectsHalfBelowFullAtOrAboveThreshold) {
    constexpr size_t kThreshold = 50;

    Simulation small = makeRandomScene(/*n=*/20, /*seed=*/11);
    small.neighborList().setAutoMode(kThreshold);
    small.neighborList().build(small.atoms(), small.world());
    EXPECT_EQ(small.neighborList().mode(), NeighborListMode::Half) << "mobileCount 20 < 50 → Half";

    Simulation big = makeRandomScene(/*n=*/80, /*seed=*/12);
    big.neighborList().setAutoMode(kThreshold);
    big.neighborList().build(big.atoms(), big.world());
    EXPECT_EQ(big.neighborList().mode(), NeighborListMode::Full) << "mobileCount 80 >= 50 → Full";

    // На границе (== threshold) — Full.
    Simulation edge = makeRandomScene(/*n=*/kThreshold, /*seed=*/13);
    edge.neighborList().setAutoMode(kThreshold);
    edge.neighborList().build(edge.atoms(), edge.world());
    EXPECT_EQ(edge.neighborList().mode(), NeighborListMode::Full) << "mobileCount == threshold → Full";
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

// 27-cell стенсил покрывает только если cellSize >= listRadius. Если cellSize
// меньше, пары на (cellSize, listRadius] могут оказаться в клетках за пределами
// 27-окна и не попасть в NL — тихая потеря пар вместо явной ошибки. Контракт
// рантайма: rebuildPipeline бросает std::invalid_argument, чтобы UI или другие
// caller'ы поймали несовместимую конфигурацию до того, как force loop начнёт
// работать на ущербном NL.
TEST(NeighborListTest, RebuildPipelineThrowsOnTooSmallCellSize) {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    // listRadius = cutoff(5) + skin(1) = 6, cellSize = 3 — заведомо меньше.
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, /*cellSize=*/3);
    sim.appendAtomFast(Vec3f{20.0f, 20.0f, 20.0f}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    sim.finalizeAtomBatch();

    EXPECT_THROW(sim.neighborList().rebuildPipeline(sim.atoms(), sim.world(), 0), std::invalid_argument);
}

// Тот же контракт должен срабатывать и на прямом вызове build() — публичного
// входа, который обходит rebuildPipeline (тесты, бенчи, будущий код). Иначе
// проверка живёт только в pipeline и build() тихо строит ущербный NL.
TEST(NeighborListTest, BuildThrowsOnTooSmallCellSize) {
    Simulation sim;
    sim.createWorld(Vec3f{40.0f, 40.0f, 40.0f});
    sim.setSizeBox(Vec3f{40.0f, 40.0f, 40.0f}, /*cellSize=*/3); // listRadius 6 > 3
    sim.appendAtomFast(Vec3f{20.0f, 20.0f, 20.0f}, Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::H);
    sim.finalizeAtomBatch();

    EXPECT_THROW(sim.neighborList().build(sim.atoms(), sim.world()), std::invalid_argument);
}
