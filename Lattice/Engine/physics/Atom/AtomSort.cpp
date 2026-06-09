#include "AtomSort.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/math/Morton3D.h"

struct AtomSort::AtomView {
    // Локальный снимок atom-record, чтобы безопасно крутить циклическую перестановку.
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 force;
    glm::vec3 prevForce;
    float energy;
    float invMass;
    float charge;
    AtomData::Type type;
    uint8_t valenceCount;
    AtomStorage::AtomId id = AtomStorage::InvalidAtomId;
};

void AtomSort::updateViewFromStorage(AtomStorage& atoms, size_t index, AtomView& view) {
    // Считываем все связанные поля атома как единую запись.
    view.pos = atoms.pos(index);
    view.vel = atoms.vel(index);
    view.force = atoms.force(index);
    view.prevForce = atoms.prevForce(index);
    view.energy = atoms.energy(index);
    view.invMass = atoms.invMass(index);
    view.charge = atoms.charge(index);
    view.type = atoms.type(index);
    view.valenceCount = atoms.valenceCount(index);
    view.id = atoms.atomId(index);
}

void AtomSort::applyViewToStorage(AtomStorage& atoms, size_t index, const AtomView& view) {
    // Возвращаем запись обратно в SoA-хранилище и синхронизируем atom id -> index.
    atoms.setPos(index, view.pos);
    atoms.setVel(index, view.vel);
    atoms.setForce(index, view.force);
    atoms.setPrevForce(index, view.prevForce);
    atoms.energy(index) = view.energy;
    atoms.invMass(index) = view.invMass;
    atoms.charge(index) = view.charge;
    atoms.type(index) = view.type;
    atoms.valenceCount(index) = view.valenceCount;
    atoms.setAtomId(index, view.id);
    // oldToNew_ строится в mortonOrder (position-indexed: oldToNew_[oldPos]=newPos, как ждёт
    // World::remapAtomIndices для связей). Здесь его НЕ трогаем: прежняя запись
    // oldToNew_[view.id]=index была id-индексированной и перезатирала позиционную карту при
    // id != position (после addAtom/removeAtom/setFixed) — ремап связей уезжал на чужие индексы.
}

void AtomSort::reorder(AtomStorage& atoms, std::span<uint32_t> indices) {
    // Переставляем только мобильные атомы циклической заменой, без отдельного полного буфера атомов.
    AtomView cycleStart;
    AtomView moved;

    constexpr uint32_t DONE_FLAG = 0x80000000u;

    for (size_t start = 0; start < atoms.mobileCount(); ++start) {
        if (indices[start] & DONE_FLAG) {
            continue;
        }

        const size_t realStart = indices[start];
        if (realStart == start) {
            indices[start] |= DONE_FLAG;
            continue;
        }

        updateViewFromStorage(atoms, start, cycleStart);
        size_t current = start;

        while (true) {
            const size_t source = indices[current] & ~DONE_FLAG;
            indices[current] |= DONE_FLAG;

            if (source == start) {
                applyViewToStorage(atoms, current, cycleStart);
                break;
            }

            updateViewFromStorage(atoms, source, moved);
            applyViewToStorage(atoms, current, moved);
            current = source;
        }
    }
}

void AtomSort::radixSort(std::vector<std::pair<uint64_t, uint32_t>>& data, std::vector<std::pair<uint64_t, uint32_t>>& buf, uint64_t maxKey) {
    // Число проходов ограничиваем реальным диапазоном ключей, чтобы не платить всегда за все 8 байт.
    int passCount = 1;
    while ((maxKey >> (passCount * 8)) != 0) {
        ++passCount;
    }

    for (int pass = 0; pass < passCount; ++pass) {
        const int shift = pass * 8;
        size_t cnt[256] = {};
        // Histogram.
        for (auto& [k, _] : data) {
            ++cnt[(k >> shift) & 0xFF];
        }

        size_t pos[256];
        pos[0] = 0;
        // Prefix sums -> bucket offsets.
        for (int i = 1; i < 256; ++i) {
            pos[i] = pos[i - 1] + cnt[i - 1];
        }

        // Stable scatter in temporary buffer.
        for (auto& p : data) {
            buf[pos[(p.first >> shift) & 0xFF]++] = p;
        }

        std::swap(data, buf);
    }
}

void AtomSort::mortonOrder(AtomStorage& atoms, const SpatialGrid& grid) {
    oldToNew_.resize(atoms.size());
    if (atoms.mobileCount() <= 1) {
        std::iota(oldToNew_.begin(), oldToNew_.end(), 0);
        return;
    }

    std::iota(oldToNew_.begin() + atoms.mobileCount(), oldToNew_.end(), static_cast<uint32_t>(atoms.mobileCount()));

    // Рабочие буферы живут в сортировщике и переиспользуются между вызовами.
    keyed_.resize(atoms.mobileCount());
    buf_.resize(atoms.mobileCount());
    indices_.resize(atoms.mobileCount());

    uint64_t maxMortonKey = 0;
    // Строим Morton-ключи по координатам ячеек сетки.
    for (size_t i = 0; i < atoms.mobileCount(); ++i) {
        uint32_t cx = std::clamp(static_cast<int>(grid.worldToCellX(atoms.posX(i))), 0, static_cast<int>(grid.size.x - 1));
        uint32_t cy = std::clamp(static_cast<int>(grid.worldToCellY(atoms.posY(i))), 0, static_cast<int>(grid.size.y - 1));
        uint32_t cz = std::clamp(static_cast<int>(grid.worldToCellZ(atoms.posZ(i))), 0, static_cast<int>(grid.size.z - 1));
        const uint64_t mortonKey = Morton3D::encode(cx, cy, cz);
        keyed_[i] = {mortonKey, static_cast<uint32_t>(i)};
        maxMortonKey = std::max(maxMortonKey, mortonKey);
    }

    // Сортируем пары {key, oldIndex}, затем извлекаем порядок индексов.
    radixSort(keyed_, buf_, maxMortonKey);

    for (size_t i = 0; i < atoms.mobileCount(); ++i) {
        indices_[i] = keyed_[i].second;
    }

    // old -> new нужен для ремапа связей и прочих внешних индексов.
    for (size_t newIdx = 0; newIdx < atoms.mobileCount(); ++newIdx) {
        oldToNew_[indices_[newIdx]] = static_cast<uint32_t>(newIdx);
    }

    // После построения перестановки физически перекладываем атомы в новом порядке.
    reorder(atoms, indices_);
}
