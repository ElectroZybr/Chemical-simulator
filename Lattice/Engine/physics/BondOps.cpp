#include "BondOps.h"

#include <algorithm>

namespace BondOps {
BondTable bondDefaultProps{};

void ensureInitialized() {
    static const bool initialized = [] {
        bondDefaultProps.init();
        return true;
    }();
    (void)initialized;
} // namespace

const BondParams* paramsFor(const AtomStorage& atomStorage, size_t aIndex, size_t bIndex, uint8_t order) {
    ensureInitialized();

    if (aIndex >= atomStorage.size() || bIndex >= atomStorage.size() || order >= kBondOrderCount) {
        return nullptr;
    }

    const BondParams& params = bondDefaultProps.get(atomStorage.type()[aIndex], atomStorage.type()[bIndex], order);
    if (params.r0 <= 0.0f || params.a <= 0.0f || params.De <= 0.0f) {
        return nullptr;
    }

    return &params;
}

Bond* create(Bond::List& bonds, size_t aIndex, size_t bIndex, uint8_t order, AtomStorage& atomStorage) {
    ensureInitialized();

    if (aIndex >= atomStorage.size() || bIndex >= atomStorage.size() || aIndex == bIndex) {
        return nullptr;
    }

    const uint8_t valenceCost = static_cast<uint8_t>(order + 1);
    if (atomStorage.valence()[aIndex] < valenceCost || atomStorage.valence()[bIndex] < valenceCost) {
        return nullptr;
    }

    if (std::ranges::any_of(bonds, [&](const Bond& bond) {
            return (bond.aIndex == aIndex && bond.bIndex == bIndex) || (bond.aIndex == bIndex && bond.bIndex == aIndex);
        })) {
        return nullptr;
    }

    const BondParams* bondParams = paramsFor(atomStorage, aIndex, bIndex, order);
    if (bondParams == nullptr) {
        return nullptr;
    }

    bonds.emplace_back(aIndex, bIndex, order, *bondParams);
    atomStorage.valence()[aIndex] -= valenceCost;
    atomStorage.valence()[bIndex] -= valenceCost;
    return &bonds.back();
}

} // namespace BondOps
