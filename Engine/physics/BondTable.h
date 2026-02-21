#pragma once

enum class AtomType {
    _,
    H,
    He,
    Li,
    Be,
    B,
    C,
    N,
    O,
    F,
    Ne,
    Na,
    Mg,
    Al,
    Si,
    P,
    S,
    Cl,
    Ar,
    // ...
    COUNT
};

struct BondParams {
    float r0=0;
    float De=0;
    float a=0;
};

struct BondTable {
    // Матрица параметров "тип1 -> тип2"
    BondParams table[(int)AtomType::COUNT][(int)AtomType::COUNT];

    void init();
    // инициализация параметров для пары
    void set(AtomType a, AtomType b, const BondParams& p) {
        table[(int)a][(int)b] = p;
        table[(int)b][(int)a] = p; // симметрия
    }

    // получить параметры
    const BondParams& get(AtomType a, AtomType b) const {
        return table[(int)a][(int)b];
    }
};

struct AngleTable {
    // Матрица параметров "тип1 -> тип2"
    BondParams table[(int)AtomType::COUNT][(int)AtomType::COUNT];

    void init();
    // инициализация параметров для пары
    void set(AtomType a, AtomType b, const BondParams& p) {
        table[(int)a][(int)b] = p;
        table[(int)b][(int)a] = p; // симметрия
    }

    // получить параметры
    const BondParams& get(AtomType a, AtomType b) const {
        return table[(int)a][(int)b];
    }
};