#pragma once

#include <cstddef>
#include <list>

#include "Lattice/Engine/physics/BondTable.h"

struct Bond {
    using List = std::list<Bond>;

    Bond(size_t aIndexIn, size_t bIndexIn, uint8_t orderIn, const BondParams& paramsIn) : aIndex(aIndexIn), bIndex(bIndexIn), order(orderIn), params(paramsIn) {}

    size_t aIndex;
    size_t bIndex;
    uint8_t order;
    BondParams params;
};
