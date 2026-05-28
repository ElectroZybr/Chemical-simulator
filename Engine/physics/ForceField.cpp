#include "ForceField.h"

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/World.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/AtomStorage.h"

#ifdef ENABLE_TBB
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#endif

namespace {
    template <bool UseLJ, bool UseCoulomb>
    void computePairInteractionsImpl(AtomStorage& atoms, const NeighborList& neighborList, const LJForceField& ljForceField,
                                     const CoulombForceField& coulombForceField) {
        const auto& offsets    = neighborList.offsets();
        const auto& neighbours = neighborList.neighbors();
        const size_t mobileN   = atoms.mobileCount();

        auto processAtom = [&](size_t atomIndex) {
            const uint32_t begin = offsets[atomIndex];
            const uint32_t end   = offsets[atomIndex + 1];
            if (begin > end || static_cast<size_t>(end) > neighbours.size()) {
                return;
            }

            const float posX = atoms.posX(atomIndex);
            const float posY = atoms.posY(atomIndex);
            const float posZ = atoms.posZ(atomIndex);
            float forceX         = atoms.forceX(atomIndex);
            float forceY         = atoms.forceY(atomIndex);
            float forceZ         = atoms.forceZ(atomIndex);
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
                        return;
                    }
                }
            }

            for (uint32_t p = begin; p < end; ++p) {
                const uint32_t bIndex = neighbours[p];
                const float dx = atoms.posX(bIndex) - posX;
                const float dy = atoms.posY(bIndex) - posY;
                const float dz = atoms.posZ(bIndex) - posZ;
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

            atoms.forceX(atomIndex) = forceX;
            atoms.forceY(atomIndex) = forceY;
            atoms.forceZ(atomIndex) = forceZ;
            atoms.energy(atomIndex) = potentialEnergy;
        };

#ifdef ENABLE_TBB
        tbb::parallel_for(tbb::blocked_range<size_t>(0, mobileN, 64),
            [&](const tbb::blocked_range<size_t>& r) {
                for (size_t i = r.begin(); i != r.end(); ++i) processAtom(i);
            });
#else
        for (size_t i = 0; i < mobileN; ++i) processAtom(i);
#endif
    }
}

ForceField::ForceField() = default;

void ForceField::compute(World& world, bool allowBondFormation, float dt) const {
    PROFILE_SCOPE("ForceField::compute");

    AtomStorage& atoms        = world.getAtomStorage();
    Bond::List& bonds         = world.getBonds();
    NeighborList& neighborList = world.getNeighborList();

    wallForceField_.compute(world);
    computePairInteractions(world);
    bondForceField_.compute(atoms, bonds, neighborList, allowBondFormation, dt);
}

void ForceField::computePairInteractions(World& world) const {
    PROFILE_SCOPE("ForceField::pairInteractions");

    AtomStorage& atoms              = world.getAtomStorage();
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
