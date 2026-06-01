#pragma once

#include <cstddef>

#include <imgui.h>

#include "Lattice/Engine/NeighborSearch/NeighborList.h"
#include "Lattice/Engine/math/Vec3.h"

class BaseRenderer;
class PickingSystem;
class SpatialGrid;
namespace Lattice {
    class Simulation;
}

class NeighborListOverlay {
public:
    void draw(const Lattice::Simulation& simulation, const PickingSystem& pickingSystem, const BaseRenderer& renderer);

private:
    size_t skinSelectedIndex_ = static_cast<size_t>(-1);
    size_t skinRebuildCount_ = static_cast<size_t>(-1);
    Vec3f skinCenter_{};

    static void drawSelectedNeighbors(const AtomStorage& atoms, const SpatialGrid& grid, const NeighborList& neighborList,
                                      const Vec3f& renderOffset, size_t selectedIndex, const BaseRenderer& renderer);
    void updateSkinCenter(size_t selectedIndex, size_t rebuildCount, Vec3f atomPos);
    static void drawWorldCircle(const BaseRenderer& renderer, Vec3f center, float radius, ImU32 color, float thickness);
};
