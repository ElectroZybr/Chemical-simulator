#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/Bond.h"

class NeighborList;

class BondForceField {
public:
    // Возвращает true, если набор связей изменился (образование/разрыв) — апстрим
    // использует этот сигнал, чтобы пометить bond-данные как "dirty" (напр. для
    // ресинка/перестроения визуализации).
    bool compute(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList, bool allowBondFormation, float dt) const;

private:
    bool formBonds(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList, Bond::Adjacency& adjacency) const;
    bool tryCreateBond(AtomStorage& atoms, Bond::List& bonds, uint32_t aIndex, uint32_t bIndex, Bond::Adjacency& adjacency) const;
    void applyAngleForces(AtomStorage& atoms, const Bond::List& bonds) const;

    // Scratch для applyAngleForces — переиспользуются между вызовами, чтобы не
    // аллоцировать std::vector(N) на каждый physics step. mutable, потому что
    // BondForceField::compute (и applyAngleForces) — const.
    mutable std::vector<uint16_t> degreeScratch_;
    mutable std::vector<std::vector<size_t>> neighborsScratch_;
    // Adjacency для O(degree) dup-check в Bond::CreateBond. Пересобирается
    // в начале compute из текущих bonds, потом передаётся в formBonds и
    // апдейтится на каждом успешном CreateBond.
    mutable Bond::Adjacency adjacencyScratch_;
};
