#include "NeighborList.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "Lattice/Engine/NeighborSearch/SpatialGrid.h"
#include "Lattice/Engine/World.h"
#include "Lattice/Engine/metrics/Profiler.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "Lattice/Engine/restrict.h"

void NeighborList::setCutoff(float cutoff) {
    cutoff_ = cutoff;
    listRadius_ = cutoff_ + skin_;
    listRadiusSqr_ = listRadius_ * listRadius_;
    valid_ = false;
}

void NeighborList::setSkin(float skin) {
    skin_ = skin;
    listRadius_ = cutoff_ + skin_;
    listRadiusSqr_ = listRadius_ * listRadius_;
    valid_ = false;
}

void NeighborList::setParams(float cutoff, float skin) {
    cutoff_ = cutoff;
    skin_ = skin;
    listRadius_ = cutoff_ + skin_;
    listRadiusSqr_ = listRadius_ * listRadius_;
    valid_ = false;
}

void NeighborList::clear() {
    neighbors_.clear();
    offsets_.clear();
    refPosX_.clear();
    refPosY_.clear();
    refPosZ_.clear();
    valid_ = false;
    resetStats();
}

void NeighborList::rebuildPipeline(AtomStorage& atoms, World& world, int simStep) {
    // сортировка атомов по ячейкам сетки
    atoms.sort(world.getGrid());
    world.remapAtomIndices(atoms.lastSortOldToNew());
    // перестройка пространственной сетки уже под новый порядок атомов
    world.getGrid().rebuild(atoms.x(), atoms.y(), atoms.z());
    // перестройка списка соседей
    build(atoms, world);
    // обновление метрик
    const float rebuildTimeMs = static_cast<float>(Profiler::instance().lastMs("NeighborList::build"));
    stats_.recordRebuild(simStep, rebuildTimeMs);
}

void NeighborList::build(const AtomStorage& atoms, World& box) {
    PROFILE_SCOPE("NeighborList::build");

    const SpatialGrid& grid = box.getGrid();
    const uint32_t atomCount = static_cast<uint32_t>(atoms.size());
    const float* RESTRICT x = atoms.x().data();
    const float* RESTRICT y = atoms.y().data();
    const float* RESTRICT z = atoms.z().data();

    reserveListBuffers(atoms);

    offsets_[0] = 0;
    for (uint32_t i = 0; i < atomCount; ++i) {
        const float xi = x[i];
        const float yi = y[i];
        const float zi = z[i];
        // запись всех соседей атома в массив
        writeAtomNeighbors(grid, x, y, z, i, xi, yi, zi, neighbors_);
        offsets_[i + 1] = neighbors_.size();
    }

    std::copy(x, x + atoms.mobileCount(), refPosX_.data());
    std::copy(y, y + atoms.mobileCount(), refPosY_.data());
    std::copy(z, z + atoms.mobileCount(), refPosZ_.data());

    valid_ = true;
}

bool NeighborList::needsRebuild(const AtomStorage& atoms) const {
    const size_t n = atoms.mobileCount();

    if (!valid_ || n != refPosX_.size()) {
        return true;
    }

    const float maxDisp = (0.5f * skin_);
    const float maxDispSqr = maxDisp * maxDisp;

    const float* RESTRICT x = atoms.x().data();
    const float* RESTRICT y = atoms.y().data();
    const float* RESTRICT z = atoms.z().data();

    const float* RESTRICT refX = refPosX_.data();
    const float* RESTRICT refY = refPosY_.data();
    const float* RESTRICT refZ = refPosZ_.data();

    for (uint32_t i = 0; i < n; ++i) {
        const float dx = x[i] - refX[i];
        const float dy = y[i] - refY[i];
        const float dz = z[i] - refZ[i];
        if ((dx * dx + dy * dy + dz * dz) > maxDispSqr) {
            return true;
        }
    }

    return false;
}

uint32_t NeighborList::atomCount() const {
    if (offsets_.empty()) {
        return 0;
    }
    return offsets_.size() - 1;
}

uint32_t NeighborList::pairStorageSize() const {
    return std::min(neighbors_.size(), static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
}

uint32_t NeighborList::memoryBytes() const {
    return neighbors_.capacity() * sizeof(uint32_t) + offsets_.capacity() * sizeof(uint32_t) + refPosX_.capacity() * sizeof(float) +
           refPosY_.capacity() * sizeof(float) + refPosZ_.capacity() * sizeof(float);
}

void NeighborList::resetStats() { stats_.reset(); }

void NeighborList::reserveListBuffers(const AtomStorage& atoms) {
    const size_t prevCapacity = neighbors_.capacity();
    neighbors_.clear();
    offsets_.resize(atoms.size() + 1);
    refPosX_.resize(atoms.mobileCount());
    refPosY_.resize(atoms.mobileCount());
    refPosZ_.resize(atoms.mobileCount());

    const size_t targetCapacity = std::max(prevCapacity, atoms.size() * 64);
    if (targetCapacity > 0) {
        neighbors_.reserve(targetCapacity);
    }
}
