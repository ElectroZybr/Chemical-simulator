#include "BondForceField.h"

#include <algorithm>
#include <vector>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/metrics/Profiler.h"

void BondForceField::compute(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList, bool allowBondFormation,
                             float dt) const {
    PROFILE_SCOPE("ForceField::Bonded");
    if (bonds.empty() && !allowBondFormation) {
        return;
    }

    // проверка образования и разрыва связей, а также расчет сил
    std::erase_if(bonds, [&](Bond& bond) {
        if (bond.shouldBreak(atoms)) {
            bond.detach(atoms);
            return true;
        }
        return false;
    });

    if (allowBondFormation) {
        formBonds(atoms, bonds, neighborList);
    }

    for (Bond& bond : bonds) {
        bond.forceBond(atoms, dt);
    }

    applyAngleForces(atoms, bonds);
}

void BondForceField::formBonds(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList) const {
    PROFILE_SCOPE("ForceField::FormBonds(NL)");
    const uint32_t atomCount = static_cast<uint32_t>(atoms.size());
    if (atomCount < 2) {
        return;
    }

    const auto& offsets = neighborList.offsets();
    const auto& neighbours = neighborList.neighbors();
    for (uint32_t atomIndex = 0; atomIndex < atomCount; ++atomIndex) {
        if (atomIndex + 1 >= offsets.size()) {
            break;
        }
        const uint32_t begin = offsets[atomIndex];
        const uint32_t end = offsets[atomIndex + 1];
        for (uint32_t p = begin; p < end; ++p) {
            tryCreateBond(atoms, bonds, atomIndex, neighbours[p]);
        }
    }
}

void BondForceField::tryCreateBond(AtomStorage& atoms, Bond::List& bonds, uint32_t aIndex, uint32_t bIndex) const {
    Bond::ensureInitialized();

    if (aIndex >= atoms.size() || bIndex >= atoms.size() || aIndex == bIndex) {
        return;
    }

    const BondParams& bondParams = Bond::bond_default_props.get(atoms.type(aIndex), atoms.type(bIndex));
    if (bondParams.r0 <= 0.0f || bondParams.a <= 0.0f || bondParams.De <= 0.0f) {
        return;
    }

    const float dx = atoms.posX(bIndex) - atoms.posX(aIndex);
    const float dy = atoms.posY(bIndex) - atoms.posY(aIndex);
    const float dz = atoms.posZ(bIndex) - atoms.posZ(aIndex);
    const float distanceSqr = dx * dx + dy * dy + dz * dz;

    const float formationDistance = std::max(2.5f, bondParams.r0 * 1.35f);
    if (distanceSqr > formationDistance * formationDistance) {
        return;
    }

    Bond::CreateBond(bonds, aIndex, bIndex, atoms);
}

void BondForceField::applyAngleForces(AtomStorage& atoms, const Bond::List& bonds) {
    if (bonds.size() < 2) {
        return;
    }

    std::vector<uint16_t> degree(atoms.size(), 0);
    for (const Bond& bond : bonds) {
        if (bond.aIndex < atoms.size() && bond.bIndex < atoms.size()) {
            ++degree[bond.aIndex];
            ++degree[bond.bIndex];
        }
    }

    std::vector<std::vector<size_t>> bondedNeighbours(atoms.size());
    for (size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
        if (degree[atomIndex] > 0) {
            bondedNeighbours[atomIndex].reserve(degree[atomIndex]);
        }
    }

    for (const Bond& bond : bonds) {
        if (bond.aIndex < atoms.size() && bond.bIndex < atoms.size()) {
            bondedNeighbours[bond.aIndex].emplace_back(bond.bIndex);
            bondedNeighbours[bond.bIndex].emplace_back(bond.aIndex);
        }
    }

    for (size_t atomIndex = 0; atomIndex < bondedNeighbours.size(); ++atomIndex) {
        const auto& neighbours = bondedNeighbours[atomIndex];
        if (neighbours.size() < 2) {
            continue;
        }

        for (size_t i = 0; i + 1 < neighbours.size(); ++i) {
            for (size_t j = i + 1; j < neighbours.size(); ++j) {
                Bond::angleForce(atoms, atomIndex, neighbours[i], neighbours[j]);
            }
        }
    }
}
