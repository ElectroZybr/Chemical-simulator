#include <algorithm>

#include "BondOps.h"
#include "Lattice/Engine/ChemistryData/BondTable.h"

namespace BondOps {

Bond* create(Bond::List& bonds, size_t aIndex, size_t bIndex, uint8_t order, AtomStorage& atomStorage, const ChemistryData& chemistryData) {
    if (aIndex >= atomStorage.size() || bIndex >= atomStorage.size() || aIndex == bIndex) {
        return nullptr;
    }

    const uint8_t valenceCost = static_cast<uint8_t>(order);
    if (atomStorage.valence()[aIndex] < valenceCost || atomStorage.valence()[bIndex] < valenceCost) {
        return nullptr;
    }

    if (std::ranges::any_of(bonds, [&](const Bond& bond) {
            return (bond.aIndex == aIndex && bond.bIndex == bIndex) || (bond.aIndex == bIndex && bond.bIndex == aIndex);
        })) {
        return nullptr;
    }

    const BondParams* bondParams = chemistryData.bondTable.get(atomStorage.type()[aIndex], atomStorage.type()[bIndex], order);
    
    if (bondParams == nullptr) {
        return nullptr;
    }

    bonds.emplace_back(aIndex, bIndex, order, *bondParams);
    atomStorage.valence()[aIndex] -= valenceCost;
    atomStorage.valence()[bIndex] -= valenceCost;
    return &bonds.back();
}

} // namespace BondOps
