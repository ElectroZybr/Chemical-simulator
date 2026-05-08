#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <unordered_set>

#include "App/interaction/selection/OverlayState.h"
#include "Engine/physics/AtomStorage.h"

class IRenderer;
class World;

struct AtomHit {
    size_t index;
    float distance;
};

class PickingSystem {
public:
    PickingSystem(AtomStorage& atomStorage, World& box, std::unique_ptr<IRenderer>& renderer);

    void clearSelection();

    bool pickAtom(Vec2i screenPos, float tolerance, AtomHit& hit) const;

    void processClick(Vec2i screenPos, bool cumulative = false);
    void processRect(Vec2i start, Vec2i end, bool cumulative = false);
    void processLasso(std::span<Vec2i> points, bool cumulative = false);

    void handleAtomRemoval(size_t removedIndex);

    const std::unordered_set<size_t>& getSelectedIndices() const { return selectedIndices; }
    const OverlayState& getOverlay() const { return overlay; }
    OverlayState& getOverlay() { return overlay; }

private:
    AtomStorage& atomStorage;
    World& box;
    std::unique_ptr<IRenderer>* renderer;
    OverlayState overlay;
    std::unordered_set<size_t> selectedIndices;

    // 2D пикинг одного атома — расстояние в экранных координатах
    bool pickAtom2D(Vec2i screenPos, float tolerance, AtomHit& hit) const;
    // 3D пикинг одного атома — ray cast
    bool pickAtom3D(Vec2i screenPos, AtomHit& hit) const;

    // Проверка точки внутри фигуры
    template <typename T> static bool pointInPolygon(Vec2<T> point, std::span<Vec2<T>> polygon);
    template <typename T> static bool pointInRect(Vec2<T> point, Vec2<T> start, Vec2<T> end);
};
