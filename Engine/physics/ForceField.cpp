#include "ForceField.h"

#include <cmath>
#include <vector>

#include "Engine/Consts.h"
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/ForceFields/CoulombForceField.h"
#include "Engine/restrict.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {
    template <bool UseLJ, bool UseCoulomb>
    void computePairInteractionsImpl(AtomStorage& atoms, NeighborList& neighborList, const LJForceField& ljForceField,
                                     const CoulombForceField& coulombForceField) {
        const size_t totalN  = atoms.size();
        const size_t mobileN = atoms.mobileCount();
        const auto& offsets    = neighborList.offsets();
        const auto& neighbours = neighborList.neighbors();

        int numThreads = 1;
#ifdef _OPENMP
        numThreads = omp_get_max_threads();
#endif

        // Каждый поток накапливает силы в свой срез массива (thread t, atom i → base + i).
        // Это исключает гонки данных при использовании третьего закона Ньютона.
        std::vector<float> tFx(numThreads * totalN, 0.0f);
        std::vector<float> tFy(numThreads * totalN, 0.0f);
        std::vector<float> tFz(numThreads * totalN, 0.0f);
        std::vector<float> tE (numThreads * totalN, 0.0f);

#pragma omp parallel for schedule(dynamic, 64)
        for (size_t atomIndex = 0; atomIndex < mobileN; ++atomIndex) {
#ifdef _OPENMP
            const int tid = omp_get_thread_num();
#else
            const int tid = 0;
#endif
            float* RESTRICT localFx = tFx.data() + tid * totalN;
            float* RESTRICT localFy = tFy.data() + tid * totalN;
            float* RESTRICT localFz = tFz.data() + tid * totalN;
            float* RESTRICT localE  = tE .data() + tid * totalN;

            const uint32_t begin = offsets[atomIndex];
            const uint32_t end   = offsets[atomIndex + 1];
            if (begin > end || static_cast<size_t>(end) > neighbours.size()) {
                continue;
            }

            const float posX = atoms.posX(atomIndex);
            const float posY = atoms.posY(atomIndex);
            const float posZ = atoms.posZ(atomIndex);
            float myFx = 0.0f, myFy = 0.0f, myFz = 0.0f, myE = 0.0f;

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

                if constexpr (UseLJ) {
                    if (d2 > Consts::Epsilon) {
                        const auto& params = (*ljPairRow)[static_cast<size_t>(atoms.type(bIndex))];
                        const float invD2  = 1.0f / d2;
                        const float invD6  = invD2 * invD2 * invD2;
                        const float invD12 = invD6 * invD6;
                        const float term6  = params.potentialC6  * invD6;
                        const float term12 = params.potentialC12 * invD12;
                        const float fScale = (12.0f * term12 - 6.0f * term6) * invD2;
                        const float potential = term12 - term6;
                        const float pfx = dx * fScale;
                        const float pfy = dy * fScale;
                        const float pfz = dz * fScale;

                        myFx -= pfx;  myFy -= pfy;  myFz -= pfz;
                        myE  += 0.5f * potential;
                        localFx[bIndex] += pfx;
                        localFy[bIndex] += pfy;
                        localFz[bIndex] += pfz;
                        localE [bIndex] += 0.5f * potential;
                    }
                }
                if constexpr (UseCoulomb) {
                    if (charge != 0.0f) {
                        const float chargeB = atoms.charge(bIndex);
                        if (chargeB != 0.0f && d2 > Consts::Epsilon) {
                            const float qqScale  = CoulombForceField::kCoulombEvAngstrom * charge * chargeB;
                            const float invR     = 1.0f / std::sqrt(d2);
                            const float fScale   = qqScale * invR / d2;
                            const float potential = qqScale * invR;
                            const float pfx = dx * fScale;
                            const float pfy = dy * fScale;
                            const float pfz = dz * fScale;

                            myFx -= pfx;  myFy -= pfy;  myFz -= pfz;
                            myE  += 0.5f * potential;
                            localFx[bIndex] += pfx;
                            localFy[bIndex] += pfy;
                            localFz[bIndex] += pfz;
                            localE [bIndex] += 0.5f * potential;
                        }
                    }
                }
            }

            localFx[atomIndex] += myFx;
            localFy[atomIndex] += myFy;
            localFz[atomIndex] += myFz;
            localE [atomIndex] += myE;
        }

        // Суммируем вклады всех потоков в атомы
        for (int t = 0; t < numThreads; ++t) {
            const size_t base = static_cast<size_t>(t) * totalN;
            for (size_t i = 0; i < mobileN; ++i) {
                atoms.forceX(i) += tFx[base + i];
                atoms.forceY(i) += tFy[base + i];
                atoms.forceZ(i) += tFz[base + i];
                atoms.energy(i) += tE [base + i];
            }
        }
    }
}

ForceField::ForceField() = default;

void ForceField::syncWalls(const SimBox& box) { wallForceField_.syncWalls(box); }

void ForceField::compute(AtomStorage& atoms, Bond::List& bonds, SimBox& box, NeighborList& neighborList, bool allowBondFormation,
                         float dt) const {
    PROFILE_SCOPE("ForceField::compute");

    wallForceField_.compute(atoms, static_force_);
    computePairInteractions(atoms, neighborList);
    bondForceField_.compute(atoms, bonds, neighborList, allowBondFormation, dt);
}

void ForceField::computePairInteractions(AtomStorage& atoms, NeighborList& neighborList) const {
    PROFILE_SCOPE("ForceField::pairInteractions");

    if (enableLJ_ && enableCoulomb_) {
        computePairInteractionsImpl<true, true>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
    else if (enableLJ_) {
        computePairInteractionsImpl<true, false>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
    else if (enableCoulomb_) {
        computePairInteractionsImpl<false, true>(atoms, neighborList, ljForceField_, coulombForceField_);
    }
}
