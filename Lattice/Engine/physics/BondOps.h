#pragma once

#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "Lattice/Engine/physics/Bond.h"

namespace BondOps {

const BondParams* paramsFor(const AtomStorage& atomStorage, size_t aIndex, size_t bIndex, uint8_t order);
Bond* create(Bond::List& bonds, size_t aIndex, size_t bIndex, uint8_t order, AtomStorage& atomStorage);

}
