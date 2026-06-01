#include "NeighborListStats.h"

#include "Engine/NeighborSearch/NeighborList.h"

[[nodiscard]] float NeighborListStats::avgNeighborsPerAtom(const NeighborList& neighborList) const {
    return neighborList.atomCount() > 0
    ? (2.0f * static_cast<float>(neighborList.pairStorageSize())) / static_cast<float>(neighborList.atomCount())
    : 0.0f;
}