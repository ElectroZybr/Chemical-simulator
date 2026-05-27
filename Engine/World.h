#pragma once

#include <string>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/Bond.h"

class World {
public:
    explicit World(Vec3f size, Vec3f renderOffset = Vec3f{0.0f, 0.0f, 0.0f});

    void clear();

    void setWorldSize(const Vec3f& newSize) {
        size = newSize;
        grid.resize(size);
    }
    const Vec3f& getWorldSize() const noexcept { return size; }

    void setRenderOffset(const Vec3f& offset) noexcept { renderOffset = offset; }
    const Vec3f& getRenderOffset() const noexcept { return renderOffset; }

    void setGravity(const Vec3f& g) { gravity = g; }
    const Vec3f& getGravity() const noexcept { return gravity; }

    void setGridCellSize(float newSize) { grid.resize(size, newSize); }
    float getGridCellSize() const noexcept { return grid.cellSize; }

    bool isLJEnabled() const { return ljEnabled; }
    void setLJEnabled(bool v) { ljEnabled = v; }
    bool isCoulombEnabled() const { return coulombEnabled; }
    void setCoulombEnabled(bool v) { coulombEnabled = v; }

    AtomStorage& getAtomStorage() noexcept { return atomStorage_; }
    const AtomStorage& getAtomStorage() const noexcept { return atomStorage_; }

    Bond::List& getBonds() noexcept { return bonds_; }
    const Bond::List& getBonds() const noexcept { return bonds_; }

    SpatialGrid& getGrid() noexcept { return grid; }
    const SpatialGrid& getGrid() const noexcept { return grid; }

    NeighborList& getNeighborList() noexcept { return neighborList_; }
    const NeighborList& getNeighborList() const noexcept { return neighborList_; }

    void addAtom(const Vec3f& start_coords, const Vec3f& start_speed, AtomData::Type type, bool fixed);
    void removeAtom(size_t atomIndex);
    void clearAtoms() { atomStorage_.clear(); };
    void clearBonds() { bonds_.clear(); }

    std::string worldTitle_;
    std::string worldDescription_;

private:
    Vec3f size;
    Vec3f renderOffset;
    Vec3f gravity;

    bool ljEnabled = true;
    bool coulombEnabled = true;

    AtomStorage atomStorage_;
    SpatialGrid grid;
    NeighborList neighborList_;
    Bond::List bonds_;
};
