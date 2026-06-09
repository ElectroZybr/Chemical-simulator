#include "CoulombForceField.h"

#include <cmath>

#include "Engine/NeighborSearch/BarnesHut/Octree.h"
#include "Engine/Consts.h"
#include "Engine/metrics/Profiler.h"

void CoulombForceField::computeLongRange(AtomStorage& atoms, const SpatialGrid& grid) const {
    PROFILE_SCOPE("CoulombForceField::compute");

    OctreeNode root;
    root.build(atoms, grid);

    constexpr float kTheta = 0.7f;
    for (size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
        if (atoms.charge(atomIndex) == 0.0f) {
            continue;
        }

        float forceX = atoms.forceX(atomIndex);
        float forceY = atoms.forceY(atomIndex);
        float forceZ = atoms.forceZ(atomIndex);
        float potentialEnergy = atoms.energy(atomIndex);

        computeForce(atoms, atomIndex, root, kTheta, forceX, forceY, forceZ, potentialEnergy);

        atoms.forceX(atomIndex) = forceX;
        atoms.forceY(atomIndex) = forceY;
        atoms.forceZ(atomIndex) = forceZ;
        atoms.energy(atomIndex) = potentialEnergy;
    }
}

void CoulombForceField::computeForce(const AtomStorage& atoms, size_t atomIndex, const OctreeNode& node, float theta, float& forceX, float& forceY, float& forceZ, float& potentialEnergy) const {
    if (node.atomCount == 0 || node.charge == 0.0f) {
        return;
    }

    const float chargeA = atoms.charge(atomIndex);
    if (chargeA == 0.0f) {
        return;
    }

    const bool isLeaf = std::all_of(std::begin(node.children), std::end(node.children), [](const auto& child) { return child == nullptr; });
    const bool containsTarget = atomIndex >= node.firstAtom && atomIndex < node.firstAtom + node.atomCount;

    if (isLeaf) {
        const float posX = atoms.posX(atomIndex);
        const float posY = atoms.posY(atomIndex);
        const float posZ = atoms.posZ(atomIndex);

        for (size_t other = node.firstAtom; other < node.firstAtom + node.atomCount; ++other) {
            if (other == atomIndex) {
                continue;
            }

            const float dx = atoms.posX(other) - posX;
            const float dy = atoms.posY(other) - posY;
            const float dz = atoms.posZ(other) - posZ;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 <= Consts::Epsilon) {
                continue;
            }

            const float qqScale = kCoulombEvAngstrom * chargeA * atoms.charge(other);
            const float invR = 1.0f / std::sqrt(d2);
            const float forceScale = qqScale * invR / d2;

            forceX -= dx * forceScale;
            forceY -= dy * forceScale;
            forceZ -= dz * forceScale;
            potentialEnergy += 0.5f * qqScale * invR;
        }
        return;
    }

    const glm::vec3 sourcePos = node.dipoleMoment / node.charge;
    const float dx = sourcePos.x - atoms.posX(atomIndex);
    const float dy = sourcePos.y - atoms.posY(atomIndex);
    const float dz = sourcePos.z - atoms.posZ(atomIndex);
    const float d2 = dx * dx + dy * dy + dz * dz;

    if (!containsTarget && d2 > Consts::Epsilon) {
        const float theta2 = theta * theta;
        if ((node.size * node.size) <= theta2 * d2) {
            const float qqScale = kCoulombEvAngstrom * chargeA * node.charge;
            const float invR = 1.0f / std::sqrt(d2);
            const float forceScale = qqScale * invR / d2;

            forceX -= dx * forceScale;
            forceY -= dy * forceScale;
            forceZ -= dz * forceScale;
            potentialEnergy += 0.5f * qqScale * invR;
            return;
        }
    }

    for (const auto& child : node.children) {
        if (child) {
            computeForce(atoms, atomIndex, *child, theta, forceX, forceY, forceZ, potentialEnergy);
        }
    }
}
