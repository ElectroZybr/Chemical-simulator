#pragma once

#include <cmath>

#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/Consts.h"
#include "Engine/physics/Atom/AtomStorage.h"

class NeighborList;
class OctreeNode;

class CoulombForceField {
public:
    void computeLongRange(AtomStorage& atoms, const SpatialGrid& grid) const;
    void computeForce(const AtomStorage& atoms, size_t atomIndex, const OctreeNode& node, float theta, float& forceX, float& forceY, float& forceZ,
                      float& potentialEnergy) const;

    static constexpr float kCoulombEvAngstrom = 140.399645f; // eV*A/e^2

    // Кулоновское взаимодействие пары: A (центр) и B (сосед bIndex).
    // d = (dx,dy,dz) = posB - posA, d2 = |d|^2. Потенциал U = qq/r, сила
    // |F| = qq/r^2 вдоль d/r, что в коде даёт qq/r^3 * d (d не нормирован).
    // При qq>0 (одноимённые заряды) сила на A направлена ОТ B — отсюда
    // forceX -= ... (отталкивание); для разноимённых qq<0 знак сам инвертирует.
    inline void pairInteraction(AtomStorage& atoms, uint32_t bIndex, float dx, float dy, float dz, float d2, float cutoffSqr, float chargeA,
                                float& forceX, float& forceY, float& forceZ, float& potentialEnergy, bool writeNeighbor = true) const {
        const float chargeB = atoms.charge(bIndex);
        if (chargeB == 0.0f) {
            return;
        }

        if (d2 <= Consts::Epsilon) {
            return; // совпадающие позиции — деление на 0, пропускаем
        }

        // Физический cutoff без ветки (C1, как в LJ): за cutoff вклад обнуляется множителем,
        // не пропуском пары — бит-в-бит как continue, но без дорогого data-dependent перехода.
        const float active = (d2 <= cutoffSqr) ? 1.0f : 0.0f;

        const float qqScale = kCoulombEvAngstrom * chargeA * chargeB;
        const float invR = 1.0f / std::sqrt(d2);
        const float forceScale = qqScale * invR / d2 * active; // qq / r^3
        const float potential = qqScale * invR * active;       // qq / r

        const float pairForceX = dx * forceScale;
        const float pairForceY = dy * forceScale;
        const float pairForceZ = dz * forceScale;
        // Половину потенциала пары на каждый атом — сумма energy() по всем
        // атомам тогда равна полной энергии без двойного счёта пары.
        const float halfPotential = 0.5f * potential;

        forceX -= pairForceX;
        forceY -= pairForceY;
        forceZ -= pairForceZ;
        potentialEnergy += halfPotential;

        if (writeNeighbor) {
            // Newton-3: на соседа — равная и противоположная сила (+pairForce).
            atoms.forceX(bIndex) += pairForceX;
            atoms.forceY(bIndex) += pairForceY;
            atoms.forceZ(bIndex) += pairForceZ;
            atoms.energy(bIndex) += halfPotential;
        }
    }
};
