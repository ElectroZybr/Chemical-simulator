#pragma once
typedef unsigned int uint;
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

    uint32_t worldToCellX(float x) const { return static_cast<uint32_t>(toCell(x, static_cast<int>(size.x))); }
    uint32_t worldToCellY(float y) const { return static_cast<uint32_t>(toCell(y, static_cast<int>(size.y))); }
    uint32_t worldToCellZ(float z) const { return static_cast<uint32_t>(toCell(z, static_cast<int>(size.z))); }

    [[nodiscard]] int countAtomsInCell(int cx, int cy, int cz) const {
        const size_t idx = static_cast<size_t>(index(cx, cy, cz));
        return static_cast<int>(offsets[idx + 1] - offsets[idx]);
    }

    [[nodiscard]] int linearCellOfAtom(uint32_t atomIndex) const noexcept { return static_cast<int>(cellIndices_[atomIndex]); }
    [[nodiscard]] const std::array<int, 27>& neighborOffsets27() const noexcept { return neighborOffsets27_; }

    [[nodiscard]] int index(int x, int y, int z) const noexcept { return (z * size.y + y) * size.x + x; }

    template <typename Func>
    void forEachNeighborCell(int cx, int cy, int cz, Func&& func) const {
        const auto& offsets27 = neighborOffsets27();
        const int center = index(cx, cy, cz);
        const int sizeX = static_cast<int>(size.x);
        const int sizeY = static_cast<int>(size.y);
        const int sizeZ = static_cast<int>(size.z);

        for (int k = 0; k < 27; ++k) {
            const int neighborCellIndex = center + offsets27[k];
            const int nx = (neighborCellIndex % sizeX + sizeX) % sizeX;
            const int ny = ((neighborCellIndex / sizeX) % sizeY + sizeY) % sizeY;
            const int nz = (neighborCellIndex / (sizeX * sizeY) + sizeZ) % sizeZ;
            func(nx, ny, nz);
        }
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
