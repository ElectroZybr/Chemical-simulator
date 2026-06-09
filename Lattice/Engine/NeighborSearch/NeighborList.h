#pragma once

#include <cstdint>
#include <vector>

#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/metrics/NeighborListStats.h"
#include "Engine/physics/Atom/AtomStorage.h"

class World;

// Half — для каждого центрального атома i хранятся только соседи с индексом
//   j < i. Подходит для serial force loop с Newton-3 записью в обе стороны
//   из одной пары.
// Full — для каждого i хранятся все соседи (как j<i, так и j>i). Каждая
//   физическая пара представлена дважды. Подходит для parallel force loop,
//   где центральный пишет только в свою forceX и никакого race с соседями.
enum class NeighborListMode {
    Half,
    Full,
};

class NeighborList {
public:
    void setCutoff(float cutoff);
    void setSkin(float skin);
    void setParams(float cutoff, float skin);
    // setMode фиксирует режим явно и выключает auto-выбор. Используется для
    // bench/diagnostics, где нужно изолировать поведение конкретного режима.
    void setMode(NeighborListMode mode);
    // setAutoMode включает выбор режима по mobileCount на каждом rebuild:
    // mobileCount < threshold → Half (меньше работы), иначе Full (parallel).
    void setAutoMode(size_t threshold);
    [[nodiscard]] NeighborListMode mode() const noexcept { return mode_; }

    void clear();
    void build(const AtomStorage& atoms, World& world);
    void rebuildPipeline(AtomStorage& atoms, World& world, int simStep);
    bool needsRebuild(const AtomStorage& atoms) const;

    [[nodiscard]] uint32_t atomCount() const;
    [[nodiscard]] uint32_t pairStorageSize() const;
    [[nodiscard]] uint32_t memoryBytes() const;
    [[nodiscard]] float cutoff() const { return cutoff_; }
    [[nodiscard]] float skin() const { return skin_; }
    [[nodiscard]] float listRadius() const { return listRadius_; }
    [[nodiscard]] bool isValid() const { return valid_; }
    [[nodiscard]] const std::vector<uint32_t>& neighbors() const { return neighbors_; }
    [[nodiscard]] const std::vector<uint32_t>& offsets() const { return offsets_; }
    [[nodiscard]] const NeighborListStats& stats() const { return stats_; }
    void resetStats();

private:
    // Единственный источник логики обхода 27-стенсила соседей атома i: для каждого
    // подходящего соседа (Half/Full + cutoff) вызывает cb(neighborIndex). Обёртки ниже
    // (count / write-в-срез / append) разделяют ЭТУ логику — без дублирования. Грид и
    // позиции читаются ТОЛЬКО для чтения → безопасно из многих потоков. Зеркалится в
    // gpu_cell_list.wgsl. cb инлайнится компилятором, накладных нет.
    template <typename OnNeighbor>
    inline void forEachNeighbor(const SpatialGrid& grid, const float* x, const float* y, const float* z,
                                const uint32_t atomIndex, const float xi, const float yi, const float zi, OnNeighbor&& cb) const {
        const auto& offsets27 = grid.neighborOffsets27();
        const int center = grid.linearCellOfAtom(atomIndex); // центральная ячейка атома i
        const bool fullMode = (mode_ == NeighborListMode::Full);

        for (int k = 0; k < 27; ++k) {
            for (uint32_t neighborIndex : grid.atomsInCell(center + offsets27[k])) {
                // atomsInCell возвращает соседей по возрастанию индексов. Half: обрываем на
                // собственном индексе (только j<i); Full: все, кроме себя.
                if (neighborIndex == atomIndex) {
                    continue;
                }
                if (!fullMode && neighborIndex >= atomIndex) {
                    break;
                }

                const float dx = x[neighborIndex] - xi;
                const float dy = y[neighborIndex] - yi;
                const float dz = z[neighborIndex] - zi;
                if (dx * dx + dy * dy + dz * dz <= listRadiusSqr_) {
                    cb(neighborIndex);
                }
            }
        }
    }

    // Число соседей атома (count-фаза параллельного build).
    inline uint32_t countAtomNeighbors(const SpatialGrid& grid, const float* x, const float* y, const float* z,
                                       const uint32_t atomIndex, const float xi, const float yi, const float zi) const {
        uint32_t count = 0;
        forEachNeighbor(grid, x, y, z, atomIndex, xi, yi, zi, [&](uint32_t) { ++count; });
        return count;
    }

    // Запись соседей атома в out[] по порядку (write-фаза параллельного build).
    inline void writeAtomNeighborsAt(const SpatialGrid& grid, const float* x, const float* y, const float* z,
                                     const uint32_t atomIndex, const float xi, const float yi, const float zi, uint32_t* out) const {
        uint32_t pos = 0;
        forEachNeighbor(grid, x, y, z, atomIndex, xi, yi, zi, [&](uint32_t n) { out[pos++] = n; });
    }

    // Дописывание соседей атома в конец вектора (серийный build, один проход).
    inline void writeAtomNeighbors(const SpatialGrid& grid, const float* x, const float* y, const float* z, const uint32_t atomIndex,
                                   const float xi, const float yi, const float zi, std::vector<uint32_t>& outNeighbors) const {
        forEachNeighbor(grid, x, y, z, atomIndex, xi, yi, zi, [&](uint32_t n) { outNeighbors.emplace_back(n); });
    }

    void reserveListBuffers(const AtomStorage& atoms);

    // uint32_t - 4 байта, максимальное количество пар в NL ~ 4 млрд
    std::vector<uint32_t> neighbors_;
    std::vector<uint32_t> offsets_;

    std::vector<float> refPosX_;
    std::vector<float> refPosY_;
    std::vector<float> refPosZ_;

    float cutoff_ = 0.0f;
    float skin_ = 0.0f;
    float listRadius_ = 0.0f;
    float listRadiusSqr_ = 0.0f;
    bool valid_ = false;
    NeighborListMode mode_ = NeighborListMode::Half;
    bool autoMode_ = false;
    size_t autoThreshold_ = 5000;
    NeighborListStats stats_{};
};
