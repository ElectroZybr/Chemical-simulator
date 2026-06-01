#include "BondForceField.h"

#include <algorithm>
#include <vector>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/metrics/Profiler.h"

bool BondForceField::compute(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList, bool allowBondFormation,
                             float dt) const {
    PROFILE_SCOPE("ForceField::Bonded");
    if (bonds.empty() && !allowBondFormation) {
        return false;
    }

    // проверка образования и разрыва связей, а также расчет сил
    bool changed = false;
    std::erase_if(bonds, [&](Bond& bond) {
        if (bond.shouldBreak(atoms)) {
            bond.detach(atoms);
            changed = true;
            return true;
        }
        return false;
    });

    if (allowBondFormation) {
        // Пересобрать adjacency из живых bonds для O(degree) dup-check в
        // последующих Bond::CreateBond. Cтоит O(N + B), но окупается на
        // больших B (где линейный скан был O(B) на каждую кандидатную пару).
        const size_t n = atoms.size();
        if (adjacencyScratch_.size() < n) {
            adjacencyScratch_.resize(n);
        }
        for (size_t i = 0; i < n; ++i) {
            adjacencyScratch_[i].clear();
        }
        for (const Bond& bond : bonds) {
            if (bond.aIndex < n && bond.bIndex < n) {
                adjacencyScratch_[bond.aIndex].push_back(static_cast<uint32_t>(bond.bIndex));
                adjacencyScratch_[bond.bIndex].push_back(static_cast<uint32_t>(bond.aIndex));
            }
        }
        changed = formBonds(atoms, bonds, neighborList, adjacencyScratch_) || changed;
    }

    for (Bond& bond : bonds) {
        bond.forceBond(atoms, dt);
    }

    applyAngleForces(atoms, bonds);
    return changed;
}

bool BondForceField::formBonds(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList,
                               Bond::Adjacency& adjacency) const {
    PROFILE_SCOPE("ForceField::FormBonds(NL)");
    const uint32_t atomCount = static_cast<uint32_t>(atoms.size());
    if (atomCount < 2) {
        return false;
    }

    const auto& offsets = neighborList.offsets();
    const auto& neighbours = neighborList.neighbors();
    bool changed = false;
    for (uint32_t atomIndex = 0; atomIndex < atomCount; ++atomIndex) {
        if (atomIndex + 1 >= offsets.size()) {
            break;
        }
        const uint32_t begin = offsets[atomIndex];
        const uint32_t end = offsets[atomIndex + 1];
        for (uint32_t p = begin; p < end; ++p) {
            changed = tryCreateBond(atoms, bonds, atomIndex, neighbours[p], adjacency) || changed;
        }
    }
    return changed;
}

bool BondForceField::tryCreateBond(AtomStorage& atoms, Bond::List& bonds, uint32_t aIndex, uint32_t bIndex,
                                   Bond::Adjacency& adjacency) const {
    Bond::ensureInitialized();

    if (aIndex >= atoms.size() || bIndex >= atoms.size() || aIndex == bIndex) {
        return false;
    }

    const BondParams& bondParams = Bond::bond_default_props.get(atoms.type(aIndex), atoms.type(bIndex));
    if (bondParams.r0 <= 0.0f || bondParams.a <= 0.0f || bondParams.De <= 0.0f) {
        return false;
    }

    const float dx = atoms.posX(bIndex) - atoms.posX(aIndex);
    const float dy = atoms.posY(bIndex) - atoms.posY(aIndex);
    const float dz = atoms.posZ(bIndex) - atoms.posZ(aIndex);
    const float distanceSqr = dx * dx + dy * dy + dz * dz;

    const float formationDistance = std::max(2.5f, bondParams.r0 * 1.35f);
    if (distanceSqr > formationDistance * formationDistance) {
        return false;
    }

    return Bond::CreateBond(bonds, aIndex, bIndex, atoms, &adjacency) != nullptr;
}

void BondForceField::applyAngleForces(AtomStorage& atoms, const Bond::List& bonds) const {
    if (bonds.size() < 2) {
        return;
    }

    const size_t n = atoms.size();

    // degreeScratch_ — счётчик степеней. resize(n, 0) делает только аллокацию при росте,
    // assign(n, 0) обнуляет и shrink не нужен. Капасити переиспользуется между шагами.
    degreeScratch_.assign(n, 0);
    for (const Bond& bond : bonds) {
        if (bond.aIndex < n && bond.bIndex < n) {
            ++degreeScratch_[bond.aIndex];
            ++degreeScratch_[bond.bIndex];
        }
    }

    // neighborsScratch_ — list[N] списков соседей. resize до n; для каждого слота
    // .clear() сохраняет capacity, поэтому повторные шаги не делают malloc/free.
    if (neighborsScratch_.size() < n) {
        neighborsScratch_.resize(n);
    }
    for (size_t i = 0; i < n; ++i) {
        neighborsScratch_[i].clear();
        if (degreeScratch_[i] > 0) {
            neighborsScratch_[i].reserve(degreeScratch_[i]);
        }
    }

    for (const Bond& bond : bonds) {
        if (bond.aIndex < n && bond.bIndex < n) {
            neighborsScratch_[bond.aIndex].emplace_back(bond.bIndex);
            neighborsScratch_[bond.bIndex].emplace_back(bond.aIndex);
        }
    }

    for (size_t atomIndex = 0; atomIndex < n; ++atomIndex) {
        const auto& neighbours = neighborsScratch_[atomIndex];
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
