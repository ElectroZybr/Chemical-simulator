#pragma once

#include "Engine/physics/AtomData.h"

struct BondParams {
    float r0 = 0;
    float De = 0;
    float a = 0;
};

struct BondTable {
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
