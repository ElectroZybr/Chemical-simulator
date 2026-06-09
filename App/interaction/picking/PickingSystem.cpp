#include "PickingSystem.h"

#include <limits>

#include "Lattice/Engine/World.h"
#include "Rendering/BaseRenderer.h"

PickingSystem::PickingSystem(AtomStorage& atomStorage, World& box, std::unique_ptr<BaseRenderer>& renderer)
    : atomStorage(&atomStorage), box(&box), renderer(&renderer) {}

void PickingSystem::setWorld(AtomStorage& newAtomStorage, World& newBox) {
    atomStorage = &newAtomStorage;
    box = &newBox;
}

void PickingSystem::clearSelection() {
    overlay.reset();
    selectedAtomIds.clear();
}

const std::unordered_set<AtomStorage::AtomId>& PickingSystem::getSelectedAtomIds() const {
    pruneInvalidSelection();
    return selectedAtomIds;
}

void PickingSystem::processClick(glm::ivec2 screenPos, bool cumulative) {
    AtomHit hit;
    bool found = pickAtom(screenPos, 10.0f, hit);

    if (found) {
        // Если атом уже был выбран и зажат Ctrl — инвертируем (снимаем выделение)
        if (cumulative && selectedAtomIds.contains(hit.id)) {
            selectedAtomIds.erase(hit.id);
        }
        else {
            // Иначе — добавляем в набор
            selectedAtomIds.insert(hit.id);
        }
    }
    else {
        // Клик в пустоту без Ctrl — сбрасываем всё
        if (!cumulative) {
            clearSelection();
        }
    }
}

void PickingSystem::processRect(glm::ivec2 start, glm::ivec2 end, bool cumulative) {
    if (!cumulative) {
        clearSelection();
    }
    BaseRenderer* rend = renderer->get();

    for (size_t i = 0; i < atomStorage->size(); ++i) {
        const glm::vec3 worldPos = displayAtomPos(i);
            const glm::ivec2 atomScreen{rend->camera.worldToScreen(worldPos)};
            if (pointInRect(atomScreen, start, end)) {
            selectedAtomIds.insert(atomStorage->atomId(i));
        }
    }
}

void PickingSystem::processLasso(std::span<glm::ivec2> points, bool cumulative) {
    if (points.size() < 3) {
        return;
    }
    if (!cumulative) {
        clearSelection();
    }
    BaseRenderer* rend = renderer->get();

    for (size_t i = 0; i < atomStorage->size(); ++i) {
        const glm::vec3 worldPos = displayAtomPos(i);
            const glm::ivec2 screenPos{rend->camera.worldToScreen(worldPos)};
            if (pointInPolygon(screenPos, points)) {
            selectedAtomIds.insert(atomStorage->atomId(i));
        }
    }
}

bool PickingSystem::pickAtom(glm::ivec2 screenPos, float tolerance, AtomHit& hit) const {
    BaseRenderer* rend = renderer->get();
    switch (rend->camera.getMode()) {
    case Camera::Mode::Mode2D:
        return pickAtom2D(screenPos, tolerance, hit);
    case Camera::Mode::Orbit:
    case Camera::Mode::Free:
        return pickAtom3D(screenPos, hit);
    }
    return false;
}

bool PickingSystem::pickAtom2D(glm::ivec2 screenPos, float tolerance, AtomHit& hit) const {
    BaseRenderer* rend = renderer->get();
    float bestDistSqr = std::numeric_limits<float>::max();
    size_t bestIndex = static_cast<size_t>(-1);

    for (size_t i = 0; i < atomStorage->size(); ++i) {
        const glm::vec3 worldPos = displayAtomPos(i);
        const glm::ivec2 atomScreen{rend->camera.worldToScreen(worldPos)};
        const glm::ivec2 d = atomScreen - screenPos;
        const float distSqr = static_cast<float>(d.x * d.x + d.y * d.y);

        // радиус атома в экранных пикселях
        const float atomRadius = AtomData::getProps(atomStorage->type(i)).radius;
        const float screenRadius = atomRadius * rend->camera.getZoom() + tolerance;

        if (distSqr < screenRadius * screenRadius && distSqr < bestDistSqr) {
            bestDistSqr = distSqr;
            bestIndex = i;
        }
    }

    if (bestIndex == static_cast<size_t>(-1)) {
        return false;
    }

    hit = {bestIndex, atomStorage->atomId(bestIndex), std::sqrt(bestDistSqr)};
    return true;
}

// 3D: ray cast — ищем ближайший атом вдоль луча
bool PickingSystem::pickAtom3D(glm::ivec2 screenPos, AtomHit& hit) const {
    const RenderRay ray = (*renderer)->camera.screenToRay(screenPos.x, screenPos.y);

    float bestT = std::numeric_limits<float>::max();
    size_t bestIndex = static_cast<size_t>(-1);

    for (size_t i = 0; i < atomStorage->size(); ++i) {
        const glm::vec3 worldPos = displayAtomPos(i);
        const float radius = AtomData::getProps(atomStorage->type(i)).radius;

        RenderRaySphereHit rayHit{};
        if (renderRaySphereIntersect(ray, worldPos, radius, rayHit)) {
            if (rayHit.t < bestT) {
                bestT = rayHit.t;
                bestIndex = i;
            }
        }
    }

    if (bestIndex == static_cast<size_t>(-1)) {
        return false;
    }

    hit = {bestIndex, atomStorage->atomId(bestIndex), bestT};
    return true;
}

glm::vec3 PickingSystem::displayAtomPos(size_t atomIndex) const {
    return atomStorage->pos(atomIndex) + box->getRenderOffset();
}

void PickingSystem::pruneInvalidSelection() const {
    if (atomStorage == nullptr || selectedAtomIds.empty()) {
        return;
    }

    for (auto it = selectedAtomIds.begin(); it != selectedAtomIds.end();) {
        if (!atomStorage->containsAtomId(*it)) {
            it = selectedAtomIds.erase(it);
        }
        else {
            ++it;
        }
    }
}

// Ray casting алгоритм для point-in-polygon
template <typename T> bool PickingSystem::pointInPolygon(const T& point, std::span<T> polygon) {
    bool inside = false;
    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    const size_t n = polygon.size();

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const int xi = static_cast<int>(polygon[i].x), yi = static_cast<int>(polygon[i].y);
        const int xj = static_cast<int>(polygon[j].x), yj = static_cast<int>(polygon[j].y);

        const bool intersects = ((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
        if (intersects) {
            inside = !inside;
        }
    }

    return inside;
}

template <typename T> bool PickingSystem::pointInRect(const T& point, const T& start, const T& end) {
    int minX = std::min(static_cast<int>(start.x), static_cast<int>(end.x));
    int maxX = std::max(static_cast<int>(start.x), static_cast<int>(end.x));
    int minY = std::min(static_cast<int>(start.y), static_cast<int>(end.y));
    int maxY = std::max(static_cast<int>(start.y), static_cast<int>(end.y));
    const int px = static_cast<int>(point.x);
    const int py = static_cast<int>(point.y);
    return (minX <= px && px <= maxX && minY <= py && py <= maxY);
}
