#include "CoulombForceField.h"

#include <cmath>

#include "Engine/Consts.h"
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/metrics/Profiler.h"

void CoulombForceField::compute(AtomStorage& atoms, NeighborList& neighborList) const {
    PROFILE_SCOPE("CoulombForceField::compute");

    // Исправление бага: этот public Coulomb-only path раньше был no-op. Теперь он
    // обходит NeighborList rows и применяет тот же cutoff, что и fused path.
    const auto& offsets = neighborList.offsets();
    const auto& neighbors = neighborList.neighbors();

    for (size_t atomIndex = 0; atomIndex < atoms.mobileCount(); ++atomIndex) {
        const uint32_t begin = offsets[atomIndex];
        const uint32_t end = offsets[atomIndex + 1];
        if (begin > end || static_cast<size_t>(end) > neighbors.size()) {
            continue;
        }

        const float charge = atoms.charge(atomIndex);
        if (charge == 0.0f) {
            continue;
        }

        const float posX = atoms.posX(atomIndex);
        const float posY = atoms.posY(atomIndex);
        const float posZ = atoms.posZ(atomIndex);
        float forceX = atoms.forceX(atomIndex);
        float forceY = atoms.forceY(atomIndex);
        float forceZ = atoms.forceZ(atomIndex);
        float potentialEnergy = atoms.energy(atomIndex);

        for (uint32_t p = begin; p < end; ++p) {
            const uint32_t bIndex = neighbors[p];
            const float dx = atoms.posX(bIndex) - posX;
            const float dy = atoms.posY(bIndex) - posY;
            const float dz = atoms.posZ(bIndex) - posZ;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 > neighborList.cutoffSqr()) {
                continue;
            }

            pairInteraction(atoms, bIndex, dx, dy, dz, d2, charge, forceX, forceY, forceZ, potentialEnergy);
        }

        atoms.forceX(atomIndex) = forceX;
        atoms.forceY(atomIndex) = forceY;
        atoms.forceZ(atomIndex) = forceZ;
        atoms.energy(atomIndex) = potentialEnergy;
    }
}
