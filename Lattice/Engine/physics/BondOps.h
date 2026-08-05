#pragma once

#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "Lattice/Engine/ChemistryData/ChemistryData.hpp"
#include "Lattice/Engine/physics/Bond.h"

namespace BondOps {
Bond* create(Bond::List& bonds, size_t aIndex, size_t bIndex, uint8_t order, AtomStorage& atomStorage, const ChemistryData& chemistryData);
}
