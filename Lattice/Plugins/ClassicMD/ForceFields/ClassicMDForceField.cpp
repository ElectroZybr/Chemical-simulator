#include "ClassicMDForceField.h"

#include <algorithm>
#include <vector>

#include "Lattice/Engine/NeighborSearch/NeighborList.h"
#include "Lattice/Engine/World.h"
#include "Lattice/Engine/metrics/Profiler.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"

namespace {
using ExclusionList = std::vector<std::vector<uint32_t>>;

ExclusionList buildExclusionList(const AtomStorage& atoms, const Bond::List& bonds) {
    ExclusionList excluded(atoms.size());

    for (const Bond& bond : bonds) {
        if (bond.aIndex >= atoms.size() || bond.bIndex >= atoms.size()) {
            continue;
        }
        excluded[bond.aIndex].push_back(static_cast<uint32_t>(bond.bIndex));
        excluded[bond.bIndex].push_back(static_cast<uint32_t>(bond.aIndex));
    }

    for (size_t center = 0; center < excluded.size(); ++center) {
        auto& bonded = excluded[center];
        std::sort(bonded.begin(), bonded.end());
        bonded.erase(std::unique(bonded.begin(), bonded.end()), bonded.end());

        for (size_t i = 0; i + 1 < bonded.size(); ++i) {
            for (size_t j = i + 1; j < bonded.size(); ++j) {
                excluded[bonded[i]].push_back(bonded[j]);
                excluded[bonded[j]].push_back(bonded[i]);
            }
        }
    }

    for (auto& row : excluded) {
        std::sort(row.begin(), row.end());
        row.erase(std::unique(row.begin(), row.end()), row.end());
    }

    return excluded;
}

bool isExcludedPair(const ExclusionList& excluded, uint32_t aIndex, uint32_t bIndex) {
    if (aIndex >= excluded.size()) {
        return false;
    }
    const auto& row = excluded[aIndex];
    return std::binary_search(row.begin(), row.end(), bIndex);
}

template <bool UseLJ, bool UseCoulomb>
void computePairInteractionsImpl(AtomStorage& atoms, const NeighborList& neighborList, const ExclusionList& excluded,
                                 const LJForceField& ljForceField, const CoulombForceField& coulombForceField) {
    const auto& offsets = neighborList.offsets();
    const auto& neighbours = neighborList.neighbors();

    for (size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
        const uint32_t begin = offsets[atomIndex];
        const uint32_t end = offsets[atomIndex + 1];
        if (begin > end || static_cast<size_t>(end) > neighbours.size()) {
            continue;
        }

        const float posX = atoms.x()[atomIndex];
        const float posY = atoms.y()[atomIndex];
        const float posZ = atoms.z()[atomIndex];
        float forceX = atoms.fx()[atomIndex];
        float forceY = atoms.fy()[atomIndex];
        float forceZ = atoms.fz()[atomIndex];
        float potentialEnergy = atoms.energy()[atomIndex];

        const LJForceField::LJPairRow* ljPairRow = nullptr;
        if constexpr (UseLJ) {
            ljPairRow = &ljForceField.pairRow(atoms.type()[atomIndex]);
        }

        float charge = 0.0f;
        if constexpr (UseCoulomb) {
            charge = atoms.charge()[atomIndex];
            if (charge == 0.0f) {
                if constexpr (!UseLJ) {
                    continue;
                }
            }
        }

        for (uint32_t p = begin; p < end; ++p) {
            const uint32_t bIndex = neighbours[p];
            if (isExcludedPair(excluded, static_cast<uint32_t>(atomIndex), bIndex)) {
                continue;
            }

            const float dx = atoms.x()[bIndex] - posX;
            const float dy = atoms.y()[bIndex] - posY;
            const float dz = atoms.z()[bIndex] - posZ;
            const float d2 = dx * dx + dy * dy + dz * dz;

            if constexpr (UseLJ) {
                ljForceField.pairInteraction(atoms, bIndex, dx, dy, dz, d2, *ljPairRow, forceX, forceY, forceZ, potentialEnergy);
            }
            if constexpr (UseCoulomb) {
                if (charge != 0.0f) {
                    coulombForceField.pairInteraction(atoms, bIndex, dx, dy, dz, d2, charge, forceX, forceY, forceZ, potentialEnergy);
                }
            }
        }

        atoms.fx()[atomIndex] = forceX;
        atoms.fy()[atomIndex] = forceY;
        atoms.fz()[atomIndex] = forceZ;
        atoms.energy()[atomIndex] = potentialEnergy;
    }
}
} // namespace

bool ClassicMDForceField::compute(World& world, const ChemistryData& chemistryData, bool allowBondFormation, float dt) const {
    PROFILE_SCOPE("ForceField::compute");

    AtomStorage& atoms = world.getAtomStorage();
    Bond::List& bonds = world.getBonds();

    wallForceField_.compute(world);
    computePairInteractions(world);

    if (world.isCoulombEnabled() && world.isCoulombLongRangeEnabled()) {
        coulombForceField_.computeLongRange(atoms, world.getGrid());
    }

    return bondForceField_.compute(atoms, bonds, world.getNeighborList(), chemistryData, allowBondFormation, dt);
}

IForceField::FieldSample ClassicMDForceField::fieldAtPoint(const AtomStorage& atoms, const SpatialGrid& grid, float x, float y, float z) const {
    const auto sample = coulombForceField_.fieldAtPoint(atoms, grid, x, y, z);
    return {
        .potential = sample.potential,
        .field = sample.field,
    };
}

void ClassicMDForceField::computePairInteractions(World& world) const {
    AtomStorage& atoms = world.getAtomStorage();
    NeighborList& neighborList = world.getNeighborList();
    const ExclusionList excluded = buildExclusionList(atoms, world.getBonds());

    if (world.isLJEnabled() && world.isCoulombEnabled()) {
        computePairInteractionsImpl<true, true>(atoms, neighborList, excluded, ljForceField_, coulombForceField_);
    }
    else if (world.isLJEnabled()) {
        computePairInteractionsImpl<true, false>(atoms, neighborList, excluded, ljForceField_, coulombForceField_);
    }
    else {
        computePairInteractionsImpl<false, true>(atoms, neighborList, excluded, ljForceField_, coulombForceField_);
    }
}
