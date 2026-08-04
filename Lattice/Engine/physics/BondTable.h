#pragma once

#include "Lattice/Engine/physics/Atom/AtomData.h"

struct BondParams {
    float r0 = 0;
    float De = 0;
    float a = 0;
};

inline constexpr uint8_t kBondOrderCount = 3;

struct BondTable {
    // Матрица параметров "тип1 -> тип2"
    BondParams table[(int)AtomData::Type::COUNT][(int)AtomData::Type::COUNT][kBondOrderCount];

    void init();
    // инициализация параметров для пары
    void set(AtomData::Type a, AtomData::Type b, uint8_t order, const BondParams& p) {
        if (order >= kBondOrderCount) {
            return;
        }
        table[(int)a][(int)b][order] = p;
        table[(int)b][(int)a][order] = p; // симметрия
    }

    // получить параметры
    const BondParams& get(AtomData::Type a, AtomData::Type b, uint8_t order) const { return table[(int)a][(int)b][order]; }
};

struct AngleTable {
    // Матрица параметров "тип1 -> тип2"
    BondParams table[(int)AtomData::Type::COUNT][(int)AtomData::Type::COUNT];

    void init();
    // инициализация параметров для пары
    void set(AtomData::Type a, AtomData::Type b, const BondParams& p) {
        table[(int)a][(int)b] = p;
        table[(int)b][(int)a] = p; // симметрия
    }

    // получить параметры
    const BondParams& get(AtomData::Type a, AtomData::Type b) const { return table[(int)a][(int)b]; }
};
