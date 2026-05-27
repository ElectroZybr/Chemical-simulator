#pragma once

#include <cstddef>

#include <imgui.h>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/math/Vec3.h"

class IRenderer;
class PickingSystem;
class SpatialGrid;
class Simulation;

class NeighborListOverlay {
public:
    void draw(const Simulation& simulation, const PickingSystem& pickingSystem, const IRenderer& renderer);

private:
    size_t skinSelectedIndex_ = static_cast<size_t>(-1);
    size_t skinRebuildCount_ = static_cast<size_t>(-1);
    Vec3f skinCenter_{};

    static void drawSelectedNeighbors(const AtomStorage& atoms, const SpatialGrid& grid, const NeighborList& neighborList,
                                      const Vec3f& renderOffset, size_t selectedIndex, const IRenderer& renderer);
    void updateSkinCenter(size_t selectedIndex, size_t rebuildCount, Vec3f atomPos);
    static void drawWorldCircle(const IRenderer& renderer, Vec3f center, float radius, ImU32 color, float thickness);
};
