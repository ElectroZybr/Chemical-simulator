#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <unordered_set>

#include "App/interaction/selection/OverlayState.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "Rendering/RenderRay.h"

class BaseRenderer;
class World;

struct AtomHit {
    size_t index;
    AtomStorage::AtomId id = AtomStorage::InvalidAtomId;
    float distance;
};

class PickingSystem {
public:
    PickingSystem(AtomStorage& atomStorage, World& box, std::unique_ptr<BaseRenderer>& renderer);

    void setWorld(AtomStorage& atomStorage, World& box);
    void clearSelection();

    bool pickAtom(glm::ivec2 screenPos, float tolerance, AtomHit& hit) const;

    void processClick(glm::ivec2 screenPos, bool cumulative = false);
    void processRect(glm::ivec2 start, glm::ivec2 end, bool cumulative = false);
    void processLasso(std::span<glm::ivec2> points, bool cumulative = false);

    const std::unordered_set<AtomStorage::AtomId>& getSelectedAtomIds() const;
    const OverlayState& getOverlay() const { return overlay; }
    OverlayState& getOverlay() { return overlay; }

private:
    AtomStorage* atomStorage;
    World* box;
    std::unique_ptr<BaseRenderer>* renderer;
    OverlayState overlay;
    mutable std::unordered_set<AtomStorage::AtomId> selectedAtomIds;

    // 2D пикинг одного атома — расстояние в экранных координатах
    bool pickAtom2D(glm::ivec2 screenPos, float tolerance, AtomHit& hit) const;
    // 3D пикинг одного атома — ray cast
    bool pickAtom3D(glm::ivec2 screenPos, AtomHit& hit) const;
    glm::vec3 displayAtomPos(size_t atomIndex) const;
    void pruneInvalidSelection() const;

    // Проверка точки внутри фигуры — поддерживаем любые типы с `.x`/`.y`
    template <typename T> static bool pointInPolygon(const T& point, std::span<T> polygon);
    template <typename T> static bool pointInRect(const T& point, const T& start, const T& end);
};
