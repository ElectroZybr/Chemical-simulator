// SpatialGrid::nonEmptyCells() — оптимизация рендера сетки (обход только непустых
// клеток вместо всех). NL её не использует (он ходит по atomsInCell соседних
// клеток), поэтому покрываем отдельно: список непустых клеток должен ТОЧНО
// совпасть с брутфорс-обходом всех клеток.

#include <cstdint>
#include <random>
#include <set>

#include <gtest/gtest.h>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"

TEST(SpatialGridTest, NonEmptyCellsMatchesBruteForce) {
    Simulation sim;
    sim.createWorld(Vec3f{60.0f, 60.0f, 60.0f});
    sim.setSizeBox(Vec3f{60.0f, 60.0f, 60.0f}, 6);

    std::mt19937 rng(123);
    std::uniform_real_distribution<float> coord(8.0f, 52.0f);
    for (int i = 0; i < 200; ++i) {
        sim.appendAtomFast(Vec3f{coord(rng), coord(rng), coord(rng)}, Vec3f{0, 0, 0}, AtomData::Type::H);
    }
    sim.finalizeAtomBatch();
    sim.neighborList().build(sim.atoms(), sim.world()); // перестраивает grid

    const SpatialGrid& grid = sim.world().getGrid();

    // Брутфорс: все клетки с countAtomsInCell > 0.
    std::set<uint32_t> expected;
    for (int z = 0; z < grid.size.z; ++z) {
        for (int y = 0; y < grid.size.y; ++y) {
            for (int x = 0; x < grid.size.x; ++x) {
                if (grid.countAtomsInCell(x, y, z) > 0) {
                    expected.insert(static_cast<uint32_t>(grid.index(x, y, z)));
                }
            }
        }
    }

    const auto cells = grid.nonEmptyCells();
    std::set<uint32_t> actual(cells.begin(), cells.end());

    EXPECT_EQ(actual, expected) << "nonEmptyCells() разошёлся с брутфорсом — рендер сетки пропустит/задвоит клетки";
    EXPECT_EQ(cells.size(), actual.size()) << "в nonEmptyCells() есть дубликаты";
    EXPECT_FALSE(expected.empty()) << "сцена должна занимать хотя бы одну клетку";
}

// После непустой перестройки rebuild на ПУСТОЙ сцене должен очистить
// nonEmptyCells(). Ветка n==0 в SpatialGrid::rebuild не должна оставлять
// устаревшие индексы (иначе рендер сетки рисует фантомные клетки удалённых
// атомов). Регресс: clear() мира при включённом рендере сетки.
TEST(SpatialGridTest, NonEmptyCellsClearedOnEmptyRebuild) {
    Simulation sim;
    sim.createWorld(Vec3f{60.0f, 60.0f, 60.0f});
    sim.setSizeBox(Vec3f{60.0f, 60.0f, 60.0f}, 6);
    for (int i = 0; i < 50; ++i) {
        sim.appendAtomFast(Vec3f{12.0f + i % 10 * 3.0f, 20.0f, 20.0f}, Vec3f{0, 0, 0}, AtomData::Type::H);
    }
    sim.finalizeAtomBatch();
    sim.neighborList().build(sim.atoms(), sim.world());
    ASSERT_FALSE(sim.world().getGrid().nonEmptyCells().empty()) << "непустая сцена должна дать непустые клетки";

    // Очистка сцены -> rebuild по пустому AtomStorage.
    sim.clear();
    sim.world().getGrid().rebuild(sim.atoms().xDataSpan(), sim.atoms().yDataSpan(), sim.atoms().zDataSpan());

    EXPECT_TRUE(sim.world().getGrid().nonEmptyCells().empty())
        << "после пустой перестройки nonEmptyCells() должен быть пуст (нет фантомных клеток)";
}
