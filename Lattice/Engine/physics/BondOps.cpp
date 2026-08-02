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

    if (aIndex >= atomStorage.size() || bIndex >= atomStorage.size()) {
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

    if (atomStorage.valence()[aIndex] <= 0 || atomStorage.valence()[bIndex] <= 0) {
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
    --atomStorage.valence()[aIndex];
    --atomStorage.valence()[bIndex];
    return &bonds.back();
}

} // namespace BondOps
