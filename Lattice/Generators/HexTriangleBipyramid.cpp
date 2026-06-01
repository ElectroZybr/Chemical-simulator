#include "HexTriangleBipyramid.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Generators {
    void triangularBipyramidCrystal(Lattice::Simulation& sim, int baseSideAtoms, AtomData::Type type, float verticalScale, double spacing, double margin) {
        baseSideAtoms = std::max(1, baseSideAtoms);
        verticalScale = std::max(0.1f, verticalScale);
        if (spacing <= 0.0) {
            spacing = static_cast<double>(AtomData::getProps(type).ljA0) * std::pow(2.0, 1.0 / 6.0);
        }

        const int baseSide = baseSideAtoms;
        const int layerRadius = baseSide - 1;
        const double rowStep = spacing * std::sqrt(3.0) * 0.5;
        const double layerShiftZ = rowStep / 3.0;
        const double layerStep = spacing * std::sqrt(2.0 / 3.0) * static_cast<double>(verticalScale);
        const Vec3f pyramidCenter(static_cast<double>(layerRadius) * spacing * 0.5, 0.0,
                                  static_cast<double>(layerRadius) * rowStep / 3.0);

        std::vector<Vec3f> positions;
        size_t atomTotal = 0;
        for (int layer = -layerRadius; layer <= layerRadius; ++layer) {
            const int side = baseSide - std::abs(layer);
            atomTotal += static_cast<size_t>(side) * static_cast<size_t>(side + 1) / 2;
        }
        positions.reserve(atomTotal);

        Vec3f maxRadius(0.0, 0.0, 0.0);

        for (int layer = -layerRadius; layer <= layerRadius; ++layer) {
            const int layerIndex = std::abs(layer);
            const int side = baseSide - layerIndex;
            const double layerShiftX = static_cast<double>(layerIndex) * spacing * 0.5;
            const double layerShiftedZ = static_cast<double>(layerIndex) * layerShiftZ;

            for (int row = 0; row < side; ++row) {
                const int rowCount = side - row;
                for (int col = 0; col < rowCount; ++col) {
                    const Vec3f latticePos(static_cast<double>(col) * spacing + static_cast<double>(row) * spacing * 0.5 + layerShiftX,
                                           static_cast<double>(layer) * layerStep,
                                           static_cast<double>(row) * rowStep + layerShiftedZ);
                    positions.push_back(latticePos);
                    const Vec3f centeredPos = latticePos - pyramidCenter;
                    maxRadius.x = std::max(maxRadius.x, std::abs(centeredPos.x));
                    maxRadius.y = std::max(maxRadius.y, std::abs(centeredPos.y));
                    maxRadius.z = std::max(maxRadius.z, std::abs(centeredPos.z));
                }
            }
        }

        sim.setSizeBox(maxRadius * 2.0 + Vec3f(margin * 2.0, margin * 2.0, margin * 2.0));
        const Vec3f boxCenter = sim.world().getWorldSize() * 0.5f;
        sim.reserveAtoms(sim.atoms().size() + positions.size());
        for (const Vec3f& position : positions) {
            sim.appendAtomFast(position - pyramidCenter + boxCenter, Vec3f(0.0, 0.0, 0.0), type);
        }

        sim.finalizeAtomBatch();
    }
}
