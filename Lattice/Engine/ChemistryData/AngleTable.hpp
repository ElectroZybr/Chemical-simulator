#pragma once
#include <unordered_map>
#include "Lattice/Engine/physics/Atom/AtomData.h"

struct AngleParams {
    float Angle; // радианы
    float stiffness;
};

struct AngleKey {
    AtomData::Type a;
    AtomData::Type b;
    AtomData::Type c;
    AtomData::Hybridization hybridization;

    bool operator==(const AngleKey& other) const {
        return a == other.a &&
               b == other.b &&
               c == other.c &&
               hybridization == other.hybridization;
    }

    struct Hash {
        size_t operator()(const AngleKey& key) const {
            return static_cast<size_t>(key.a)
                | (static_cast<size_t>(key.b) << 8)
                | (static_cast<size_t>(key.c) << 16)
                | (static_cast<size_t>(key.hybridization) << 24);
        }
    };
};

struct AngleTable {
    std::unordered_map<AngleKey, AngleParams, AngleKey::Hash> table;

    void init();

    void set(AtomData::Type a, AtomData::Type b, AtomData::Type c, AtomData::Hybridization hybridization, float Angle, float stiffness) {
        table[{a, b, c, hybridization}] = {Angle, stiffness};
    }

    const AngleParams* get(AngleKey key) const {
        // свап, например C-C-H = H-C-C
        if (key.a > key.c) {
            std::swap(key.a, key.c);
        }
        auto it = table.find(key);
        if (it == table.end()) {
            return nullptr;
        }

        return &it->second;
    }
};
