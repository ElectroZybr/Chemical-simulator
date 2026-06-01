#include "LJForceField.h"

#include <cmath>

#include "Engine/Consts.h"
#include "Engine/metrics/Profiler.h"

LJForceField::LJForceField() : ljPairTable_(buildLJPairTable()) {}

LJForceField::LJPairTable LJForceField::buildLJPairTable() {
    /* построение таблицы LJ-параметров */
    LJPairTable table{};
    constexpr int typeCount = static_cast<int>(table.size());

    for (int i = 0; i < typeCount; ++i) {
        const auto& pi = AtomData::getProps(static_cast<AtomData::Type>(i));
        const float a0i = pi.ljA0;
        const float epsi = pi.ljEps;

        for (int j = i; j < typeCount; ++j) {
            const auto& pj = AtomData::getProps(static_cast<AtomData::Type>(j));
            const float a0j = pj.ljA0;
            const float epsj = pj.ljEps;

            LJParams params{};
            const float a0 = 0.5f * (a0i + a0j);
            const float eps = std::sqrt(epsi * epsj);
            const float a2 = a0 * a0;
            const float a6 = a2 * a2 * a2;
            const float a12 = a6 * a6;

            params.potentialC6 = 4.0f * eps * a6;
            params.potentialC12 = 4.0f * eps * a12;

            table[i][j] = params;
            table[j][i] = params;
        }
    }
    return table;
}

const LJForceField::LJPairRow& LJForceField::pairRow(AtomData::Type type) const {
    return ljPairTable_[static_cast<size_t>(type)];
}