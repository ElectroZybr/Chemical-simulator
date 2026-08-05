#pragma once

#include "Lattice/Engine/physics/Atom/AtomData.h"

struct BondParams {
    float r0 = 0;
    float De = 0;
    float a = 0;
};

inline constexpr uint8_t kBondOrderCount = 4;

struct BondTable {
    // Матрица параметров "тип1 -> тип2"
    BondParams table[(uint8_t)AtomData::Type::COUNT][(uint8_t)AtomData::Type::COUNT][kBondOrderCount];

    void init();
    // инициализация параметров для пары
    void set(AtomData::Type a, AtomData::Type b, AtomData::Order order, const BondParams& p) {
        if ((uint8_t)order - 1 >= kBondOrderCount) {
            return;
        }
        table[(uint8_t)a][(uint8_t)b][(uint8_t)order - 1] = p;
        table[(uint8_t)b][(uint8_t)a][(uint8_t)order - 1] = p; // симметрия
    }
    const BondParams* get(AtomData::Type a, AtomData::Type b, uint8_t order) const { return &table[(uint8_t)a][(uint8_t)b][order - 1]; }
};