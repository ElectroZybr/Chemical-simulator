#include "Massive.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Generators {
    namespace detail {
        inline float randomUnit() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }

        inline glm::vec3 randomVelocity(float scale) {
            return glm::vec3(randomUnit() * 2.0f - 1.0f, randomUnit() * 2.0f - 1.0f, randomUnit() * 2.0f - 1.0f) * scale;
        }

        enum class Axis : uint8_t {
            X,
            Y,
            Z,
        };

        struct CrystalLayout {
            Axis first;
            Axis second;
            Axis depth;
        };

        CrystalLayout layoutFor(CrystalPlane plane) {
            switch (plane) {
            case CrystalPlane::XY:
                return {Axis::X, Axis::Y, Axis::Z};
            case CrystalPlane::XZ:
                return {Axis::X, Axis::Z, Axis::Y};
            case CrystalPlane::YZ:
                return {Axis::Y, Axis::Z, Axis::X};
            }
            return {Axis::X, Axis::Y, Axis::Z};
        }

        void setAxis(glm::vec3& vector, Axis axis, double value) {
            const auto component = static_cast<float>(value);
            switch (axis) {
            case Axis::X:
                vector.x = component;
                break;
            case Axis::Y:
                vector.y = component;
                break;
            case Axis::Z:
                vector.z = component;
                break;
            }
        }

        int getAxis(const glm::ivec3& vector, Axis axis) {
            switch (axis) {
            case Axis::X:
                return vector.x;
            case Axis::Y:
                return vector.y;
            case Axis::Z:
                return vector.z;
            }
            return vector.x;
        }

        glm::vec3 makeLayoutVector(CrystalLayout layout, double first, double second, double depth) {
            glm::vec3 vector(0.0f);
            setAxis(vector, layout.first, first);
            setAxis(vector, layout.second, second);
            setAxis(vector, layout.depth, depth);
            return vector;
        }

        glm::vec3 makeCrystalBoxSize(double side, bool is3d, CrystalLayout layout) {
            constexpr double flatSide = 6.0;
            return makeLayoutVector(layout, side, side, is3d ? side : flatSide);
        }

        double makeCrystalSpan(int count, double padding, double margin) {
            return static_cast<double>(count) * padding + padding + 2.0 * margin;
        }

        glm::vec3 makeCrystalMargin(bool is3d, CrystalLayout layout, double margin) {
            return makeLayoutVector(layout, margin, margin, is3d ? margin : 0.0);
        }
    }

    void massive(Lattice::Simulation& sim, int n, AtomData::Type type, bool is3d, CrystalPlane plane, double padding, double margin) {
        massive(sim, glm::ivec3(n, n, n), type, is3d, plane, padding, margin);
    }

    void massive(Lattice::Simulation& sim, glm::ivec3 count, AtomData::Type type, bool is3d, CrystalPlane plane, double padding, double margin) {
        const detail::CrystalLayout layout = detail::layoutFor(plane);
        count = glm::max(count, glm::ivec3(1));

        const int firstCount = detail::getAxis(count, layout.first);
        const int secondCount = detail::getAxis(count, layout.second);
        const int depthCount = is3d ? detail::getAxis(count, layout.depth) : 1;

        sim.setSizeBox(detail::makeLayoutVector(layout, detail::makeCrystalSpan(firstCount, padding, margin),
                                                detail::makeCrystalSpan(secondCount, padding, margin),
                                                is3d ? detail::makeCrystalSpan(depthCount, padding, margin) : 6.0));

        const glm::vec3 vecMargin = detail::makeCrystalMargin(is3d, layout, margin);
        const size_t atomTotal = static_cast<size_t>(firstCount) * static_cast<size_t>(secondCount) * static_cast<size_t>(depthCount);
        sim.reserveAtoms(sim.atoms().size() + atomTotal);

        for (int first = 1; first <= firstCount; ++first) {
            for (int second = 1; second <= secondCount; ++second) {
                for (int depth = 1; depth <= depthCount; ++depth) {
                    const glm::vec3 latticeCoords = detail::makeLayoutVector(layout, first, second, depth);
                    sim.appendAtomFast(latticeCoords * static_cast<float>(padding) + vecMargin, detail::randomVelocity(0.5f), type);
                }
            }
        }

        sim.finalizeAtomBatch();
    }

    void AngularVelocity(Lattice::Simulation& sim, glm::vec3 angularVelocity) {
        const glm::vec3 center = sim.world().getWorldSize() * 0.5f;
        AtomStorage& atoms = sim.atoms();

        for (size_t atomIndex = 0; atomIndex < atoms.mobileCount(); ++atomIndex) {
            const glm::vec3 radial = atoms.pos(atomIndex) - center;
            atoms.setVel(atomIndex, glm::cross(angularVelocity, radial));
        }
    }
}
