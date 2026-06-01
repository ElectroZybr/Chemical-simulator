#include "HexLattice.h"

namespace Generators {
    void hexLattice(Lattice::Simulation& sim, Vec3f count, AtomData::Type type, float start_force, float margin) {
        const size_t atomTotal = static_cast<size_t>(count.x) * static_cast<size_t>(count.y) * static_cast<size_t>(count.z);

        const float lj_min = AtomData::getProps(type).ljA0 * std::pow(2.0f, 1.0f / 6.0f);
        const float rowStep = lj_min * std::sqrt(3.0f) * 0.5f;
        const float layerShiftY = lj_min * std::sqrt(3.0f) / 6.0f;
        const float layerStep = lj_min * std::sqrt(2.0f / 3.0f);

        sim.setSizeBox(Vec3f(2.0f * margin + count.x * lj_min + 1.5f * lj_min, 2.0f * margin + count.y * rowStep + 1.5f * lj_min,
                             2.0f * margin + count.z * layerStep + lj_min));

        sim.reserveAtoms(sim.atoms().size() + atomTotal);
        for (int z = 0; z < count.z; ++z) {
            const bool isBLayer = (z % 2) == 1;
            for (int y = 0; y < count.y; ++y) {
                const bool oddRow = (y % 2) == 1;
                const float xOffset = (oddRow ? 0.5f * lj_min : 0.0f) + (isBLayer ? 0.5f * lj_min : 0.0f) + lj_min;
                const float yCoord = (margin + y * rowStep + (isBLayer ? layerShiftY : 0.0f)) + lj_min;
                const float zCoord = (margin + z * layerStep) + 2;

                for (int x = 0; x < count.x; ++x) {
                    const float xCoord = margin + x * lj_min + xOffset;
                    sim.appendAtomFast(Vec3f(xCoord, yCoord, zCoord), Vec3f::Random() * start_force, type);
                }
            }
        }
        sim.finalizeAtomBatch();
    }
}
