#pragma once

#include <algorithm>
#include <cmath>

#include "Engine/Consts.h"
#include "Engine/physics/AtomStorage.h"

class NeighborList;

class CoulombForceField {
public:
    void compute(AtomStorage& atoms, NeighborList& neighborList) const;

    static constexpr float kCoulombEvAngstrom = 140.399645f; // eV*A/e^2

    inline void pairInteraction(AtomStorage& atoms, uint32_t bIndex, float dx, float dy, float dz, float d2, float chargeA, float& forceX,
                                float& forceY, float& forceZ, float& potentialEnergy) const {
        const float chargeB = atoms.charge(bIndex);
        if (chargeB == 0.0f) {
            return;
        }

        if (d2 <= Consts::Epsilon) {
            return;
        }

        // Исправление бага: overlapping charged atoms раньше создавали force
        // spikes из invR / d2; общий floor сохраняет finite math.
        const float safeD2 = std::max(d2, Consts::MinPairDistanceSqr);
        const float qqScale = kCoulombEvAngstrom * chargeA * chargeB;
        const float invR = 1.0f / std::sqrt(safeD2);
        const float forceScale = qqScale * invR / safeD2;
        const float potential = qqScale * invR;

        const float pairForceX = dx * forceScale;
        const float pairForceY = dy * forceScale;
        const float pairForceZ = dz * forceScale;

        forceX -= pairForceX;
        forceY -= pairForceY;
        forceZ -= pairForceZ;

        atoms.forceX(bIndex) += pairForceX;
        atoms.forceY(bIndex) += pairForceY;
        atoms.forceZ(bIndex) += pairForceZ;

        potentialEnergy += 0.5f * potential;
        atoms.energy(bIndex) += 0.5f * potential;
    }
};
