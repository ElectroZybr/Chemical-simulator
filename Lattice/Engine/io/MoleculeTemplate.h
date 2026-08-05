#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include <glm/glm.hpp>

#include "Lattice/Engine/physics/Atom/AtomData.h"

struct MoleculeAtom {
    AtomData::Type type;
    glm::vec3 localPos;
    AtomData::Hybridization hybridization;
};

struct MoleculeBond {
    // индексы атомов в std::vector<MoleculeAtom>
    uint32_t atomA = 0;
    uint32_t atomB = 0;
    uint8_t order = 0;
};

struct MoleculeTemplate {
    std::string name;
    std::vector<MoleculeAtom> atoms;
    std::vector<MoleculeBond> bonds;
};