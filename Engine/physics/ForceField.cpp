#include "ForceField.h"

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/AtomStorage.h"

namespace {
    // Исправление бага: rebuild до integration пропускал быстрые атомы после
    // движения. Force-entry refresh ловит predicted positions и external
    // AtomStorage mutations до использования neighbors в pair/bond force.
    void refreshNeighborListIfNeeded(World& world, int simStep) {
        AtomStorage& atoms = world.getAtomStorage();
        NeighborList& neighborList = world.getNeighborList();

        if (neighborList.needsRebuild(atoms)) {
            neighborList.rebuildPipeline(atoms, world, simStep);
        }
    }

    template <bool UseLJ, bool UseCoulomb>
    void computePairInteractionsImpl(AtomStorage& atoms, const NeighborList& neighborList, const LJForceField& ljForceField,
                                     const CoulombForceField& coulombForceField) {
        const auto& offsets = neighborList.offsets();
        const auto& neighbours = neighborList.neighbors();

        for (size_t atomIndex = 0; atomIndex < atoms.mobileCount(); ++atomIndex) {
            const uint32_t begin = offsets[atomIndex];
            const uint32_t end = offsets[atomIndex + 1];
            if (begin > end || static_cast<size_t>(end) > neighbours.size()) {
                continue;
            }

            const float posX = atoms.posX(atomIndex);
            const float posY = atoms.posY(atomIndex);
            const float posZ = atoms.posZ(atomIndex);
            float forceX = atoms.forceX(atomIndex);
            float forceY = atoms.forceY(atomIndex);
            float forceZ = atoms.forceZ(atomIndex);
            float potentialEnergy = atoms.energy(atomIndex);

            const LJForceField::LJPairRow* ljPairRow = nullptr;
            if constexpr (UseLJ) {
                ljPairRow = &ljForceField.pairRow(atoms.type(atomIndex));
            }

            float charge = 0.0f;
            if constexpr (UseCoulomb) {
                charge = atoms.charge(atomIndex);
                if (charge == 0.0f) {
                    if constexpr (!UseLJ) {
                        continue;
                    }
                }
            }

            for (uint32_t p = begin; p < end; ++p) {
                const uint32_t bIndex = neighbours[p];
                const float dx = atoms.posX(bIndex) - posX;
                const float dy = atoms.posY(bIndex) - posY;
                const float dz = atoms.posZ(bIndex) - posZ;
                const float d2 = dx * dx + dy * dy + dz * dz;
                // Исправление бага: NeighborList включает cutoff + skin для
                // rebuild amortization, но LJ/Coulomb physics использует real cutoff.
                if (d2 > neighborList.cutoffSqr()) {
                    continue;
                }

                if constexpr (UseLJ) {
                    ljForceField.pairInteraction(atoms, bIndex, dx, dy, dz, d2, *ljPairRow, forceX, forceY, forceZ, potentialEnergy);
                }
                if constexpr (UseCoulomb) {
                    if (charge != 0.0f) {
                        coulombForceField.pairInteraction(atoms, bIndex, dx, dy, dz, d2, charge, forceX, forceY, forceZ, potentialEnergy);
                    }
                }
            }

            atoms.forceX(atomIndex) = forceX;
            atoms.forceY(atomIndex) = forceY;
            atoms.forceZ(atomIndex) = forceZ;
            atoms.energy(atomIndex) = potentialEnergy;
        }
    }
}

ForceField::ForceField() = default;

void ForceField::compute(World& world, bool allowBondFormation, float dt, int simStep) const {
    PROFILE_SCOPE("ForceField::compute");

    AtomStorage& atoms = world.getAtomStorage();
    Bond::List& bonds = world.getBonds();
    NeighborList& neighborList = world.getNeighborList();
    refreshNeighborListIfNeeded(world, simStep);

    wallForceField_.compute(world);
    if (world.isLJEnabled() && world.isCoulombEnabled()) {
        computePairInteractionsImpl<true, true>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
    else if (world.isLJEnabled()) {
        computePairInteractionsImpl<true, false>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
    else if (world.isCoulombEnabled()) {
        computePairInteractionsImpl<false, true>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
    bondForceField_.compute(atoms, bonds, neighborList, allowBondFormation, dt);
}

void ForceField::computePairInteractions(World& world) const {
    PROFILE_SCOPE("ForceField::pairInteractions");

    refreshNeighborListIfNeeded(world, 0);

    AtomStorage& atoms = world.getAtomStorage();
    const NeighborList& neighborList = world.getNeighborList();

    if (world.isLJEnabled() && world.isCoulombEnabled()) {
        computePairInteractionsImpl<true, true>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
    else if (world.isLJEnabled()) {
        computePairInteractionsImpl<true, false>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
    else if (world.isCoulombEnabled()) {
        computePairInteractionsImpl<false, true>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
}
