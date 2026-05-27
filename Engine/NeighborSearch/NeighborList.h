#pragma once

#include <cstdint>
#include <vector>

#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/metrics/NeighborListStats.h"
#include "Engine/physics/AtomStorage.h"

class World;

class NeighborList {
public:
    void setCutoff(float cutoff);
    void setSkin(float skin);
    void setParams(float cutoff, float skin);

    void clear();
    void build(const AtomStorage& atoms, World& world);
    void rebuildPipeline(const AtomStorage& atoms, World& world, int simStep);
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

    // Hot-path helper для записи соседей одного атома.
    inline void writeAtomNeighbors(const SpatialGrid& grid, const float* x, const float* y, const float* z, const uint32_t atomIndex,
                                   const float xi, const float yi, const float zi, std::vector<uint32_t>& outNeighbors) const {
        const auto& offsets27 = grid.neighborOffsets27();
        const int center = grid.linearCellOfAtom(atomIndex); // центральная ячейка атома i

        for (int k = 0; k < 27; ++k) {
            for (uint32_t neighborIndex : grid.atomsInCell(center + offsets27[k])) {
                // grid.atomsInCellByLinearIndex возвращает соседей в порядке возрастания индексов
                if (neighborIndex >= atomIndex) {
                    break;
                }

                const float dx = x[neighborIndex] - xi;
                const float dy = y[neighborIndex] - yi;
                const float dz = z[neighborIndex] - zi;
                if (dx * dx + dy * dy + dz * dz <= listRadiusSqr_) {
                    outNeighbors.emplace_back(neighborIndex);
                }
            }
        }
    }

private:
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
    NeighborListStats stats_{};
};
