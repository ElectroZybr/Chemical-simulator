#include "ForceField.h"
#include "Engine/World.h"
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/AtomStorage.h"

#ifdef LATTICELAB_USE_TBB
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#endif

namespace {
    // Параллельный режим (TBB + Full NL) рентабелен только когда работы достаточно
    // для амортизации overhead парallel_for setup. повторное профилирование рекомендовало
    // эмпирический порог 3-8k; начали с 5k и оставили серийный путь для меньших N.
    constexpr size_t kParallelMobileThreshold = 5000;

    template <bool UseLJ, bool UseCoulomb>
    inline void processAtomNeighbors(AtomStorage& atoms, const std::vector<uint32_t>& offsets, const std::vector<uint32_t>& neighbours,
                                     const LJForceField& ljForceField, const CoulombForceField& coulombForceField, size_t atomIndex,
                                     float cutoffSqr, bool writeNeighbor) {
        const uint32_t begin = offsets[atomIndex];
        const uint32_t end = offsets[atomIndex + 1];
        if (begin > end || static_cast<size_t>(end) > neighbours.size()) {
            return;
        }

        // Fixed-атомы декоративны (AtomStorage::addAtom doc): они не оказывают
        // pair-сил на мобильных. В Half NL это вытекало автоматически
        // (j<i + force loop до mobileCount), в Full NL fixed может попасть как
        // сосед мобильного, поэтому фильтр явный.
        const size_t mobileCount = atoms.mobileCount();

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
                    return;
                }
            }
        }

        for (uint32_t p = begin; p < end; ++p) {
            const uint32_t bIndex = neighbours[p];
            if (bIndex >= mobileCount) {
                // fixed neighbor — пропускаем, см. addAtom doc выше.
                continue;
            }
            const float dx = atoms.posX(bIndex) - posX;
            const float dy = atoms.posY(bIndex) - posY;
            const float dz = atoms.posZ(bIndex) - posZ;
            const float d2 = dx * dx + dy * dy + dz * dz;

            if (d2 > cutoffSqr) {
                continue;
            }

            if constexpr (UseLJ) {
                ljForceField.pairInteraction(atoms, bIndex, dx, dy, dz, d2, *ljPairRow, forceX, forceY, forceZ, potentialEnergy,
                                             writeNeighbor);
            }
            if constexpr (UseCoulomb) {
                if (charge != 0.0f) {
                    coulombForceField.pairInteraction(atoms, bIndex, dx, dy, dz, d2, charge, forceX, forceY, forceZ, potentialEnergy,
                                                      writeNeighbor);
                }
            }
        }

        atoms.forceX(atomIndex) = forceX;
        atoms.forceY(atomIndex) = forceY;
        atoms.forceZ(atomIndex) = forceZ;
        atoms.energy(atomIndex) = potentialEnergy;
    }

    template <bool UseLJ, bool UseCoulomb>
    void computePairInteractionsImpl(AtomStorage& atoms, const NeighborList& neighborList, const LJForceField& ljForceField,
                                     const CoulombForceField& coulombForceField) {
        const auto& offsets = neighborList.offsets();
        const auto& neighbours = neighborList.neighbors();
        // NL хранит пары до listRadius = cutoff + skin. Физическая сила должна
        // обрезаться по cutoff; skin — это только запас, чтобы реже перестраивать NL.
        const float cutoff = neighborList.cutoff();
        const float cutoffSqr = cutoff * cutoff;
        const size_t mobileCount = atoms.mobileCount();
        // Half NL хранит каждую пару один раз (j<i) → Newton-3 запись соседа.
        // Full NL хранит каждую пару дважды (один раз с каждой стороны) → запись
        // только в центральный, иначе сила удвоится.
        const bool fullMode = (neighborList.mode() == NeighborListMode::Full);
        const bool writeNeighbor = !fullMode;

#ifdef LATTICELAB_USE_TBB
        if (fullMode && mobileCount >= kParallelMobileThreshold) {
            tbb::parallel_for(
                tbb::blocked_range<size_t>(0, mobileCount),
                [&](const tbb::blocked_range<size_t>& range) {
                    for (size_t i = range.begin(); i != range.end(); ++i) {
                        processAtomNeighbors<UseLJ, UseCoulomb>(atoms, offsets, neighbours, ljForceField, coulombForceField, i,
                                                                cutoffSqr, writeNeighbor);
                    }
                });
            return;
        }
#endif

        for (size_t atomIndex = 0; atomIndex < mobileCount; ++atomIndex) {
            processAtomNeighbors<UseLJ, UseCoulomb>(atoms, offsets, neighbours, ljForceField, coulombForceField, atomIndex, cutoffSqr,
                                                    writeNeighbor);
        }
    }
}

ForceField::ForceField() = default;

bool ForceField::compute(World& world, bool allowBondFormation, float dt) const {
    PROFILE_SCOPE("ForceField::compute");

    AtomStorage& atoms = world.getAtomStorage();
    Bond::List& bonds = world.getBonds();
    NeighborList& neighborList = world.getNeighborList();

    wallForceField_.compute(world);
    computePairInteractions(world);
    return bondForceField_.compute(atoms, bonds, neighborList, allowBondFormation, dt);
}

void ForceField::computePairInteractions(World& world) const {
    PROFILE_SCOPE("ForceField::pairInteractions");

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
