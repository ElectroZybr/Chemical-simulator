#pragma once

#include <cmath>

#include <glm/glm.hpp>

#include "Lattice/Engine/NeighborSearch/SpatialGrid.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "Lattice/Engine/Consts.h"

class NeighborList;
class OctreeNode;

class CoulombForceField {
public:
    struct FieldSample {
        float potential = 0.0f;
        glm::vec3 field{0.0f};
    };

    void computeLongRange(AtomStorage& atoms, const SpatialGrid& grid) const;
    void computeForce(const AtomStorage& atoms, size_t atomIndex, const OctreeNode& node, float theta, float& forceX, float& forceY, float& forceZ, float& potentialEnergy) const;
    FieldSample fieldAtPoint(const AtomStorage& atoms, const SpatialGrid& grid, float x, float y, float z) const;
    static constexpr float kCoulombEvAngstrom = 14.399645f; // eV*A/e^2

    inline void pairInteraction(AtomStorage& atoms, uint32_t bIndex, float dx, float dy, float dz, float d2, float chargeA, float& forceX,
                                float& forceY, float& forceZ, float& potentialEnergy) const {
        const float chargeB = atoms.charge()[bIndex];
        if (chargeB == 0.0f) {
            return;
        }

        if (d2 <= Consts::Epsilon) {
            return;
        }

        const float qqScale = kCoulombEvAngstrom * chargeA * chargeB;
        const float invR = 1.0f / std::sqrt(d2);
        const float forceScale = qqScale * invR / d2;
        const float potential = qqScale * invR;

        const float pairForceX = dx * forceScale;
        const float pairForceY = dy * forceScale;
        const float pairForceZ = dz * forceScale;

        forceX -= pairForceX;
        forceY -= pairForceY;
        forceZ -= pairForceZ;

        atoms.fx()[bIndex] += pairForceX;
        atoms.fy()[bIndex] += pairForceY;
        atoms.fz()[bIndex] += pairForceZ;

        potentialEnergy += 0.5f * potential;
        atoms.energy()[bIndex] += 0.5f * potential;
    }
};
