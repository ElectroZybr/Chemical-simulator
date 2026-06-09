#include "SpatialGrid.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "Engine/metrics/Profiler.h"

SpatialGrid::SpatialGrid(const glm::vec3& worldSize, float cellSize) : cellSize(cellSize) {
    if (worldSize.x < 0 || worldSize.y < 0 || worldSize.z < 0) {
        throw std::invalid_argument("SpatialGrid::SpatialGrid: size must be > 0");
    }
    if (this->cellSize <= 0) {
        throw std::invalid_argument("SpatialGrid::SpatialGrid: cellSize must be > 0");
    }

    const float cellsX = std::max<float>(1.f + 2.f * kGhostLayers, (worldSize.x + this->cellSize - 1.f) / this->cellSize + 2.f * kGhostLayers);
    const float cellsY = std::max<float>(1.f + 2.f * kGhostLayers, (worldSize.y + this->cellSize - 1.f) / this->cellSize + 2.f * kGhostLayers);
    const float cellsZ = std::max<float>(1.f + 2.f * kGhostLayers, (worldSize.z + this->cellSize - 1.f) / this->cellSize + 2.f * kGhostLayers);
    this->size = glm::uvec3(static_cast<uint32_t>(cellsX), static_cast<uint32_t>(cellsY), static_cast<uint32_t>(cellsZ));
    countCells = this->size.x * this->size.y * this->size.z;

    offsets.assign(countCells + 1, 0);
    rebuildNeighborOffsets();
    counts_.assign(countCells, 0);
    cellIndices_.reserve(256);
    atomsInCells.reserve(256);
}

// Перестроение cell-list через counting sort за O(n) — три прохода без
// сравнений и без per-cell аллокаций:
//   1) каждому атому считаем его клетку и инкрементим счётчик клетки counts_;
//   2) prefix-sum counts_ -> offsets (начало диапазона каждой клетки в
//      atomsInCells); попутно собираем индексы непустых клеток для
//      sparse-обхода рендером;
//   3) раскладываем атомы: после prefix-sum counts_[cell] равен началу клетки
//      и переиспользуется как курсор записи.
void SpatialGrid::rebuild(std::span<const float> posX, std::span<const float> posY, std::span<const float> posZ) {
    PROFILE_SCOPE("SpatialGrid::rebuild");
    const size_t n = posX.size();

    if (n == 0) {
        atomsInCells.clear();
        std::fill(offsets.begin(), offsets.end(), 0);
        nonEmptyCellIndices_.clear(); // иначе рендер сетки рисует фантомные клетки удалённых атомов
        stats_.recordRebuild(0, 0, 0.0f);
        return;
    }

    cellIndices_.resize(n);
    counts_.assign(countCells, 0);

    // Проход 1: клетка каждого атома + счётчик атомов на клетку.
    for (size_t i = 0; i < n; ++i) {
        const size_t cell = static_cast<size_t>(index(worldToCellX(posX[i]), worldToCellY(posY[i]), worldToCellZ(posZ[i])));
        cellIndices_[i] = cell;
        ++counts_[cell];
    }

    // Проход 2: prefix-sum счётчиков в offsets; counts_[cell] переустанавливаем
    // в начало клетки (станет курсором в проходе 3), копим непустые клетки.
    offsets.resize(countCells + 2);
    uint32_t running = 0;
    size_t nonEmptyCellCount = 0;
    uint32_t maxAtomsPerCell = 0;
    nonEmptyCellIndices_.clear();
    for (size_t cell = 0; cell < countCells; ++cell) {
        const uint32_t cnt = counts_[cell];
        if (cnt > 0) {
            nonEmptyCellIndices_.push_back(static_cast<uint32_t>(cell));
            ++nonEmptyCellCount;
        }
        maxAtomsPerCell = std::max(counts_[cell], maxAtomsPerCell);
        counts_[cell] = running;
        offsets[cell] = running;
        running += cnt;
    }
    offsets[countCells] = running;

    // Проход 3: counts_[cell] здесь — курсор записи (после prefix-sum указывает
    // на начало клетки), ++ сдвигает его на следующую свободную позицию.
    atomsInCells.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const size_t cell = cellIndices_[i];
        atomsInCells[counts_[cell]++] = i;
    }

    const float averageAtomsPerNonEmptyCell = nonEmptyCellCount > 0 ? static_cast<float>(n) / static_cast<float>(nonEmptyCellCount) : 0.0f;
    stats_.recordRebuild(nonEmptyCellCount, maxAtomsPerCell, averageAtomsPerNonEmptyCell);
}

void SpatialGrid::resize(const glm::vec3& newWorldSize, float newCellSize) {
    if (newCellSize > 0) {
        cellSize = newCellSize;
    }
    else if (newCellSize <= 0 && newCellSize != -1) {
        throw std::invalid_argument("SpatialGrid::resize: newCellSize must be > 0");
    }

    auto calculateCells = [this](float worldDim) -> uint32_t {
        float num = std::ceil(worldDim / this->cellSize);
        return static_cast<uint32_t>(std::max(1.0f, num));
    };

    uint32_t ghostPadding = static_cast<uint32_t>(kGhostLayers) * 2;

    this->size.x = calculateCells(newWorldSize.x) + ghostPadding;
    this->size.y = calculateCells(newWorldSize.y) + ghostPadding;
    this->size.z = calculateCells(newWorldSize.z) + ghostPadding;

    countCells = static_cast<size_t>(this->size.x) * this->size.y * this->size.z;

    offsets.assign(countCells + 1, 0);
    atomsInCells.clear();
    rebuildNeighborOffsets();
    stats_.reset();
}

void SpatialGrid::rebuildNeighborOffsets() noexcept {
    /* построение массива смещений для 27 соседей */
    uint32_t k = 0;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                neighborOffsets27_[k++] = dx + (dy + dz * size.y) * size.x;
            }
        }
    }
}

size_t SpatialGrid::memoryBytes() const {
    return atomsInCells.capacity() * sizeof(atomsInCells[0]) + offsets.capacity() * sizeof(offsets[0]) + sizeof(neighborOffsets27_) +
           cellIndices_.capacity() * sizeof(cellIndices_[0]) + counts_.capacity() * sizeof(counts_[0]);
}
