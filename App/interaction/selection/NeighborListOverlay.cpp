#include "NeighborListOverlay.h"

#include <cmath>

#include <imgui.h>

#include "App/interaction/picking/PickingSystem.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/Simulation.h"
#include "Rendering/BaseRenderer.h"

namespace {
    constexpr int kCircleSegments = 96;
    constexpr float kLinkThickness = 1.0f;
    constexpr float kRadiusThickness = 2.0f;
    constexpr ImU32 kLinkColor = IM_COL32(130, 190, 255, 155);
    constexpr ImU32 kCutoffColor = IM_COL32(80, 220, 140, 210);
    constexpr ImU32 kSkinColor = IM_COL32(255, 190, 80, 210);

    ImVec2 toImVec2(Vec2i v) {
        return ImVec2(static_cast<float>(v.x), static_cast<float>(v.y));
    }
}

void NeighborListOverlay::draw(const Simulation& simulation, const PickingSystem& pickingSystem, const IRenderer& renderer) {
    if (renderer.camera.getMode() != Camera::Mode::Mode2D) {
        return;
    }

    const AtomStorage& atoms = simulation.atoms();
    const World& world = simulation.world();
    const NeighborList& neighborList = simulation.neighborList();

    const auto& selectedIndices = pickingSystem.getSelectedIndices();
    if (selectedIndices.size() != 1) {
        return;
    }

    const size_t selectedIndex = *selectedIndices.begin();
    if (selectedIndex >= atoms.size()) {
        return;
    }

    const Vec3f atomPos = atoms.pos(selectedIndex) + world.getRenderOffset();
    updateSkinCenter(selectedIndex, neighborList.stats().rebuildCount(), atomPos);

    if (neighborList.isValid()) {
        drawSelectedNeighbors(atoms, world.getGrid(), neighborList, world.getRenderOffset(), selectedIndex, renderer);
    }

    drawWorldCircle(renderer, atomPos, neighborList.cutoff(), kCutoffColor, kRadiusThickness);
    drawWorldCircle(renderer, skinCenter_, neighborList.listRadius(), kSkinColor, kRadiusThickness);
}

void NeighborListOverlay::drawSelectedNeighbors(const AtomStorage& atoms, const SpatialGrid& grid, const NeighborList& neighborList,
                                                const Vec3f& renderOffset, size_t selectedIndex, const IRenderer& renderer) {
    if (selectedIndex >= atoms.size()) {
        return;
    }

    const Vec3f selectedPos = atoms.pos(selectedIndex);
    const Vec3f selectedDisplayPos = selectedPos + renderOffset;
    const ImVec2 selectedScreen = toImVec2(renderer.camera.worldToScreen(selectedDisplayPos));
    const float listRadiusSqr = neighborList.listRadius() * neighborList.listRadius();
    const int centerCell = grid.linearCellOfAtom(static_cast<uint32_t>(selectedIndex));
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    for (int offset : grid.neighborOffsets27()) {
        for (uint32_t neighborIndex : grid.atomsInCell(centerCell + offset)) {
            if (neighborIndex == selectedIndex || neighborIndex >= atoms.size()) {
                continue;
            }

            const Vec3f delta = atoms.pos(neighborIndex) - selectedPos;
            if (delta.sqrAbs() <= listRadiusSqr) {
                const ImVec2 neighborScreen = toImVec2(renderer.camera.worldToScreen(atoms.pos(neighborIndex) + renderOffset));
                dl->AddLine(selectedScreen, neighborScreen, kLinkColor, kLinkThickness);
            }
        }
    }
}

void NeighborListOverlay::updateSkinCenter(size_t selectedIndex, size_t rebuildCount, Vec3f atomPos) {
    if (skinSelectedIndex_ == selectedIndex && skinRebuildCount_ == rebuildCount) {
        return;
    }

    skinSelectedIndex_ = selectedIndex;
    skinRebuildCount_ = rebuildCount;
    skinCenter_ = atomPos;
}

void NeighborListOverlay::drawWorldCircle(const IRenderer& renderer, Vec3f center, float radius, ImU32 color, float thickness) {
    if (radius <= 0.0f) {
        return;
    }

    const Vec2i screenCenter = renderer.camera.worldToScreen(center);
    const float screenRadius = radius * renderer.camera.getZoom();
    if (screenRadius <= 0.5f || !std::isfinite(screenRadius)) {
        return;
    }

    ImGui::GetForegroundDrawList()->AddCircle(toImVec2(screenCenter), screenRadius, color, kCircleSegments, thickness);
}
