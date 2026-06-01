#include "NeighborList.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/World.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/restrict.h"

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

void NeighborList::setMode(NeighborListMode mode) {
    autoMode_ = false;
    if (mode_ != mode) {
        mode_ = mode;
        valid_ = false;
    }
}

void NeighborList::setAutoMode(size_t threshold) {
    if (!autoMode_ || autoThreshold_ != threshold) {
        autoMode_ = true;
        autoThreshold_ = threshold;
        valid_ = false;
    }
}

void NeighborList::clear() {
    neighbors_.clear();
    offsets_.clear();
    refPosX_.clear();
    refPosY_.clear();
    refPosZ_.clear();
    neighbors_.shrink_to_fit();
    offsets_.shrink_to_fit();
    refPosX_.shrink_to_fit();
    refPosY_.shrink_to_fit();
    refPosZ_.shrink_to_fit();
    valid_ = false;
    resetStats();
}

void NeighborList::rebuildPipeline(const AtomStorage& atoms, World& world, int simStep) {
    // перестройка пространственной сетки
    world.getGrid().rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());
    // перестройка списка соседей (контракт cellSize >= listRadius проверяется в build)
    build(atoms, world);
    // обновление метрик
    const float rebuildTimeMs = static_cast<float>(Profiler::instance().lastMs("NeighborList::build"));
    stats_.recordRebuild(simStep, rebuildTimeMs);
}

void NeighborList::build(const AtomStorage& atoms, World& box) {
    PROFILE_SCOPE("NeighborList::build");

    // 27-cell стенсил покрывает только если cellSize >= listRadius. Иначе пары на
    // расстоянии (cellSize, listRadius] окажутся за пределами обхода и тихо выпадут
    // из NL — force loop их не учтёт. Это контракт, а не warning. Проверяем здесь,
    // на публичном входе build(), а не только в rebuildPipeline — иначе прямой
    // вызов build() (тесты, бенчи, будущий код) обходит контракт.
    const float cellSize = box.getGrid().cellSize;
    if (cellSize + 1e-6f < listRadius_) {
        throw std::invalid_argument(
            "NeighborList::build: cellSize must be >= listRadius (cutoff + skin); "
            "27-cell stencil cannot cover the radius otherwise");
    }

    // Auto-mode: режим выбирается по mobileCount на каждом rebuild.
    // На малых сценах Half дешевле (1x работа в force loop, нет 2x памяти NL);
    // на больших — Full окупает 2x работу parallel-выгодой.
    if (autoMode_) {
        mode_ = (atoms.mobileCount() >= autoThreshold_) ? NeighborListMode::Full : NeighborListMode::Half;
    }

    const SpatialGrid& grid = box.getGrid();
    const uint32_t atomCount = static_cast<uint32_t>(atoms.size());
    const float* RESTRICT x = atoms.xData();
    const float* RESTRICT y = atoms.yData();
    const float* RESTRICT z = atoms.zData();

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

    const float* RESTRICT x = atoms.xData();
    const float* RESTRICT y = atoms.yData();
    const float* RESTRICT z = atoms.zData();

    const float* RESTRICT refX = refPosX_.data();
    const float* RESTRICT refY = refPosY_.data();
    const float* RESTRICT refZ = refPosZ_.data();

    int rebuild = false;
#pragma GCC ivdep
    for (uint32_t i = 0; i < n; ++i) {
        const float dx = x[i] - refX[i];
        const float dy = y[i] - refY[i];
        const float dz = z[i] - refZ[i];
        rebuild |= ((dx * dx + dy * dy + dz * dz) > maxDispSqr);
    }

    return rebuild;
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

    // первый build — fallback, потом реальный размер из прошлого раза
    if (prevCapacity > 0) {
        neighbors_.reserve(prevCapacity);
    }
    else {
        neighbors_.reserve(atoms.size() * 64);
    }
}
