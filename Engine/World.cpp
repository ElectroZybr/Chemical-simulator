#include "World.h"

World::World(Vec3f size, Vec3f renderOffset) : size(size), renderOffset(renderOffset), grid(size) { atomStorage_.reserve(250000); }

void World::clear() {
    clearAtoms();
    clearBonds();
    neighborList_.clear();
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
}

void World::addAtom(const Vec3f& start_coords, const Vec3f& start_speed, AtomData::Type type, bool fixed) {
    atomStorage_.addAtom(start_coords, start_speed, type, fixed);
    // Исправление бага: изменение числа/позиции атомов делает кэшированные
    // rows NeighborList устаревшими, поэтому список очищается перед rebuild grid.
    neighborList_.clear();
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
}

void World::removeAtom(size_t atomIndex) {
    if (atomIndex >= atomStorage_.size()) {
        return;
    }

    const size_t lastIndex = atomStorage_.size() - 1;

    for (auto it = bonds_.begin(); it != bonds_.end();) {
        if (it->aIndex == atomIndex || it->bIndex == atomIndex) {
            if (it->aIndex == atomIndex && it->bIndex != atomIndex && it->bIndex < atomStorage_.size()) {
                ++atomStorage_.valenceCount(it->bIndex);
            }
            if (it->bIndex == atomIndex && it->aIndex != atomIndex && it->aIndex < atomStorage_.size()) {
                ++atomStorage_.valenceCount(it->aIndex);
            }
            it = bonds_.erase(it);
            continue;
        }

        if (atomIndex != lastIndex) {
            if (it->aIndex == lastIndex) {
                it->aIndex = atomIndex;
            }
            if (it->bIndex == lastIndex) {
                it->bIndex = atomIndex;
            }
        }

        ++it;
    }

    atomStorage_.removeAtom(atomIndex);
    // Исправление бага: удаление меняет местами rows в storage и устаревает
    // offsets NeighborList; очищаем кэшированный список перед следующим force query.
    neighborList_.clear();
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
}
