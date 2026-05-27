#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "Engine/Consts.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"

class NeighborList;

class LJForceField {
public:
    struct LJParams {
        float potentialC6 = 0.0f;
        float potentialC12 = 0.0f;
    };

    static constexpr size_t TypeCount = static_cast<size_t>(AtomData::Type::COUNT);
    using LJPairTable = std::array<std::array<LJParams, TypeCount>, TypeCount>;
    using LJPairRow = std::array<LJParams, TypeCount>;

    LJForceField();

    [[nodiscard]] const LJPairRow& pairRow(AtomData::Type type) const;

    inline void pairInteraction(AtomStorage& atoms, uint32_t bIndex, float dx, float dy, float dz, float d2, const LJPairRow& ljPairRow,
                                float& forceX, float& forceY, float& forceZ, float& potenE) const {
        if (d2 <= Consts::Epsilon) {
            return;
        }

        const LJParams& params = ljPairRow[static_cast<size_t>(atoms.type(bIndex))];

        // Исправление бага: near-overlap LJ pairs могли численно взрываться.
        // Clamp применяется только к kernel distance, не меняя normal-distance interactions.
        const float safeD2 = std::max(d2, Consts::MinPairDistanceSqr);
        const float invD2 = 1.0f / safeD2;
        const float invD6 = invD2 * invD2 * invD2;
        const float invD12 = invD6 * invD6;

        const float term6 = params.potentialC6 * invD6;
        const float term12 = params.potentialC12 * invD12;

        const float forceScale = (12.0f * term12 - 6.0f * term6) * invD2;
        const float potential = term12 - term6;

        const float pairForceX = dx * forceScale;
        const float pairForceY = dy * forceScale;
        const float pairForceZ = dz * forceScale;

        forceX -= pairForceX;
        forceY -= pairForceY;
        forceZ -= pairForceZ;

        atoms.forceX(bIndex) += pairForceX;
        atoms.forceY(bIndex) += pairForceY;
        atoms.forceZ(bIndex) += pairForceZ;

        potenE += 0.5f * potential;
        atoms.energy(bIndex) += 0.5f * potential;
    }

private:
    static LJPairTable buildLJPairTable();

    LJPairTable ljPairTable_;
};
