#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include "Engine/metrics/SpatialGridStats.h"

class SpatialGrid {
public:
    glm::uvec3 size{0u};
    float cellSize;
    size_t countCells;

    SpatialGrid(const glm::vec3& worldSize, float cellSize = 6.f);

    void rebuild(std::span<const float> posX, std::span<const float> posY, std::span<const float> posZ);
    void resize(const glm::vec3& newWorldSize, float newCellSize = -1);

    // метрики SG
    [[nodiscard]] const SpatialGridStats& stats() const noexcept { return stats_; }
    [[nodiscard]] size_t memoryBytes() const;

    // (warning) нет проверки выхода за границы
    [[nodiscard]] std::span<const uint32_t> atomsInCell(int x, int y, int z) const noexcept { return atomsInCell(index(x, y, z)); }

    // (warning) нет проверки выхода за границы
    [[nodiscard]] std::span<const uint32_t> atomsInCell(size_t linearIndex) const noexcept {
        const size_t begin = offsets[linearIndex];
        return std::span<const uint32_t>(atomsInCells.data() + begin, offsets[linearIndex + 1] - begin);
    }

    uint32_t worldToCellX(float x) const { return toCell(x, size.x); }
    uint32_t worldToCellY(float y) const { return toCell(y, size.y); }
    uint32_t worldToCellZ(float z) const { return toCell(z, size.z); }

    [[nodiscard]] int countAtomsInCell(int cx, int cy, int cz) const {
        const size_t idx = static_cast<size_t>(index(cx, cy, cz));
        return static_cast<int>(offsets[idx + 1] - offsets[idx]);
    }

    [[nodiscard]] int linearCellOfAtom(uint32_t atomIndex) const noexcept { return static_cast<int>(cellIndices_[atomIndex]); }
    [[nodiscard]] const std::array<int, 27>& neighborOffsets27() const noexcept { return neighborOffsets27_; }

    [[nodiscard]] int index(int x, int y, int z) const noexcept { return (z * size.y + y) * size.x + x; }

    // Линейные индексы непустых клеток. Заполняется в rebuild и переиспользуется
    // для render-обхода сетки: без него drawGridImpl шёл O(всех клеток) даже
    // для разреженных сцен (на 100k клетках это ms-долго).
    [[nodiscard]] std::span<const uint32_t> nonEmptyCells() const noexcept {
        return {nonEmptyCellIndices_.data(), nonEmptyCellIndices_.size()};
    }

private:
    static constexpr uint32_t kGhostLayers = 1;

    // CSR хранение данных
    std::vector<uint32_t> offsets;      // массив оффсетов (каждый оффсет - начало новой ячейки)
    std::vector<uint32_t> atomsInCells; // атомы подряд сгруппированные по ячейкам
    std::array<int, 27> neighborOffsets27_{};

    // рабочие буферы rebuild — переиспользуются между вызовами
    std::vector<uint32_t> cellIndices_;
    std::vector<uint32_t> counts_;
    std::vector<uint32_t> nonEmptyCellIndices_;

    SpatialGridStats stats_{};

    void rebuildNeighborOffsets() noexcept;

    [[nodiscard]] bool inBounds(int x, int y, int z) const noexcept {
        return x >= 0 && x < size.x && y >= 0 && y < size.y && z >= 0 && z < size.z;
    }

    [[nodiscard]] int toCell(float coord, int size) const {
        const int c = static_cast<int>(coord / cellSize) + 1;
        return std::clamp(c, 1, std::max(1, size - 2));
    }
};
