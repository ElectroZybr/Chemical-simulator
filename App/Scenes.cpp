#include "Scenes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>
#include <vector>

namespace Scenes {
    namespace detail {
        Vec3f makeCrystalBoxSize(double side, bool is3d, CrystalPlane plane) {
            if (is3d) {
                return Vec3f(side, side, side);
            }

            constexpr double flatSide = 6.0;
            switch (plane) {
            case CrystalPlane::XY:
                return Vec3f(side, side, flatSide);
            case CrystalPlane::XZ:
                return Vec3f(side, flatSide, side);
            case CrystalPlane::YZ:
                return Vec3f(flatSide, side, side);
            default:
                return Vec3f(side, side, flatSide);
            }
        }

        Vec3f makeCrystalCoords(int first, int second, int depth, CrystalPlane plane) {
            switch (plane) {
            case CrystalPlane::XY:
                return Vec3f(first, second, depth);
            case CrystalPlane::XZ:
                return Vec3f(first, depth, second);
            case CrystalPlane::YZ:
                return Vec3f(depth, first, second);
            default:
                return Vec3f(first, second, depth);
            }
        }

        Vec3f makeCrystalMargin(bool is3d, CrystalPlane plane, double margin) {
            if (is3d) {
                return Vec3f(margin, margin, margin);
            }

            switch (plane) {
            case CrystalPlane::XY:
                return Vec3f(margin, margin, 0.0);
            case CrystalPlane::XZ:
                return Vec3f(margin, 0.0, margin);
            case CrystalPlane::YZ:
                return Vec3f(0.0, margin, margin);
            default:
                return Vec3f(margin, margin, 0.0);
            }
        }

        bool hasNeighborInStorage(const Simulation& sim, const Vec3f& coords, float delta) {
            const World& box = sim.world();
            const AtomStorage& atoms = sim.atoms();
            const int cx = box.getGrid().worldToCellX(coords.x);
            const int cy = box.getGrid().worldToCellY(coords.y);
            const int cz = box.getGrid().worldToCellZ(coords.z);
            const float deltaSqr = delta * delta;
            const int radiusCells = std::max(1, static_cast<int>(std::ceil(delta / static_cast<float>(box.getGrid().cellSize))));

            for (int dz = -radiusCells; dz <= radiusCells; ++dz) {
                for (int dy = -radiusCells; dy <= radiusCells; ++dy) {
                    for (int dx = -radiusCells; dx <= radiusCells; ++dx) {
                        const int nx = cx + dx;
                        const int ny = cy + dy;
                        const int nz = cz + dz;
                        if (nx < 0 || ny < 0 || nz < 0 || nx >= box.getGrid().size.x || ny >= box.getGrid().size.y ||
                            nz >= box.getGrid().size.z) {
                            continue;
                        }

                        std::span<const uint32_t> cell = box.getGrid().atomsInCell(nx, ny, nz);
                        for (size_t atomIndex : cell) {
                            if (atomIndex >= atoms.size()) {
                                continue;
                            }
                            if ((coords - atoms.pos(atomIndex)).sqrAbs() < deltaSqr) {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }

        uint32_t resolveSeed(uint32_t seed) { return seed == 0 ? std::random_device{}() : seed; }
    }

    void crystal(Simulation& sim, int n, AtomData::Type type, bool is3d, CrystalPlane plane, double padding, double margin) {
        const double side = n * padding + padding + 2.0 * margin;

        sim.setSizeBox(detail::makeCrystalBoxSize(side, is3d, plane));

        const Vec3f vecMargin = detail::makeCrystalMargin(is3d, plane, margin);
        const int depthMax = is3d ? n : 1;
        const size_t atomTotal = static_cast<size_t>(n) * static_cast<size_t>(n) * static_cast<size_t>(depthMax);
        sim.reserveAtoms(sim.atoms().size() + atomTotal);

        for (int first = 1; first <= n; ++first) {
            for (int second = 1; second <= n; ++second) {
                for (int depth = 1; depth <= depthMax; ++depth) {
                    const Vec3f latticeCoords = detail::makeCrystalCoords(first, second, depth, plane);
                    sim.appendAtomFast(latticeCoords * padding + vecMargin, Vec3f::Random() * 0.5f, type);
                }
            }
        }

        sim.finalizeAtomBatch();
    }

    void triangularBipyramidCrystal(Simulation& sim, int baseSideAtoms, AtomData::Type type, float verticalScale, double spacing, double margin) {
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

    void AngularVelocity(Simulation& sim, Vec3f angularVelocity) {
        const Vec3f center = sim.world().getWorldSize() * 0.5f;
        AtomStorage& atoms = sim.atoms();

        for (size_t atomIndex = 0; atomIndex < atoms.mobileCount(); ++atomIndex) {
            const Vec3f radial = atoms.pos(atomIndex) - center;
            atoms.setVel(atomIndex, angularVelocity.cross(radial));
        }
    }

    void hexLattice(Simulation& sim, Vec3f count, AtomData::Type type, float start_force, float margin) {
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

    int randomGasInCurrentBox(Simulation& sim, int atomCount, AtomData::Type type, bool is3d, float minDistance, float speedScale,
                              int maxAttemptsPerAtom, uint32_t seed) {
        atomCount = std::max(0, atomCount);
        if (atomCount == 0) {
            sim.neighborList().clear();
            return 0;
        }

        std::srand(static_cast<unsigned>(detail::resolveSeed(seed)));

        const World& world = sim.world();
        const float minDistanceSqr = minDistance * minDistance;

        const size_t oldSize = sim.atoms().size();
        std::vector<Vec3f> acceptedPositions;
        acceptedPositions.reserve(static_cast<size_t>(atomCount));

        std::vector<std::vector<Vec3f>> pendingByCell(static_cast<size_t>(world.getGrid().countCells));
        const int pendingRadiusCells = std::max(1, static_cast<int>(std::ceil(minDistance / static_cast<float>(world.getGrid().cellSize))));

        const auto isTooCloseToPending = [&](const Vec3f& coords) {
            const int cx = world.getGrid().worldToCellX(coords.x);
            const int cy = world.getGrid().worldToCellY(coords.y);
            const int cz = world.getGrid().worldToCellZ(coords.z);

            for (int dz = -pendingRadiusCells; dz <= pendingRadiusCells; ++dz) {
                for (int dy = -pendingRadiusCells; dy <= pendingRadiusCells; ++dy) {
                    for (int dx = -pendingRadiusCells; dx <= pendingRadiusCells; ++dx) {
                        const int nx = cx + dx;
                        const int ny = cy + dy;
                        const int nz = cz + dz;
                        if (nx < 0 || ny < 0 || nz < 0 || nx >= world.getGrid().size.x || ny >= world.getGrid().size.y ||
                            nz >= world.getGrid().size.z) {
                            continue;
                        }

                        const int cellIndex = world.getGrid().index(nx, ny, nz);
                        const auto& bucket = pendingByCell[static_cast<size_t>(cellIndex)];
                        for (const Vec3f& other : bucket) {
                            if ((coords - other).sqrAbs() < minDistanceSqr) {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        };

        const double zMid = world.getWorldSize().z * 0.5;
        const double zSpan = world.getWorldSize().z - 4.0;
        const int maxZ = std::max(0, static_cast<int>(zSpan));
        for (int i = 0; i < atomCount; ++i) {
            for (int attempt = 0; attempt < maxAttemptsPerAtom; ++attempt) {
                const double rx = std::rand() % int(world.getWorldSize().x - 4.0);
                const double ry = std::rand() % int(world.getWorldSize().y - 4.0);
                const double rz = is3d ? (std::rand() % (maxZ + 1)) : zMid;
                const Vec3f coords(rx + 2.0, ry + 2.0, is3d ? (rz + 2.0) : zMid);

                if (!detail::hasNeighborInStorage(sim, coords, minDistance) && !isTooCloseToPending(coords)) {
                    acceptedPositions.emplace_back(coords);
                    const int cell = world.getGrid().index(world.getGrid().worldToCellX(coords.x), world.getGrid().worldToCellY(coords.y),
                                                           world.getGrid().worldToCellZ(coords.z));
                    pendingByCell[static_cast<size_t>(cell)].emplace_back(coords);
                    break;
                }
            }
        }

        if (acceptedPositions.empty()) {
            sim.neighborList().clear();
            return 0;
        }

        sim.reserveAtoms(oldSize + acceptedPositions.size());
        for (const Vec3f& pos : acceptedPositions) {
            const Vec3f randomSpeed = Vec3f::Random() * speedScale;
            sim.appendAtomFast(pos, is3d ? randomSpeed : Vec3f(randomSpeed.x, randomSpeed.y, 0.0f), type);
        }

        sim.finalizeAtomBatch();
        return static_cast<int>(acceptedPositions.size());
    }

    void randomGas(Simulation& sim, int atomCount, AtomData::Type type, bool is3d, double spacing, double margin, float density,
                   float speedScale, uint32_t seed) {
        atomCount = std::max(0, atomCount);
        const float clampedDensity = std::clamp(density, 0.25f, 3.0f);
        const double effectiveSpacing = spacing / static_cast<double>(clampedDensity);

        const int sideCount = is3d ? std::max(1, static_cast<int>(std::ceil(std::cbrt(static_cast<double>(atomCount)))))
                                   : std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(atomCount)))));

        const double span = sideCount * effectiveSpacing + 2.0 * margin;

        sim.setSizeBox(Vec3f(span, span, is3d ? span : 6));

        randomGasInCurrentBox(sim, atomCount, type, is3d, 4.0f, speedScale, 20, seed);
    }

    void randomGasMixed(Simulation& sim, int totalAtomCount, const std::vector<AtomTypeSpec>& atomSpecs, bool is3d, double spacing,
                        double margin, float density, float speedScale, uint32_t seed) {
        if (atomSpecs.empty() || totalAtomCount <= 0) {
            return;
        }

        // Вычисляем количество атомов каждого типа
        int remainingAtoms = totalAtomCount;
        std::vector<int> atomCountsPerType(atomSpecs.size(), 0);
        bool useConcentration = false;
        float totalConcentration = 0.0f;

        // Первый проход: определяем, используются ли проценты
        for (const auto& spec : atomSpecs) {
            if (spec.concentrationPercent > 0.0f) {
                useConcentration = true;
                totalConcentration += spec.concentrationPercent;
            }
        }

        // Вычисляем количество атомов для каждого типа
        if (useConcentration) {
            // Используем проценты
            for (size_t i = 0; i < atomSpecs.size(); ++i) {
                if (atomSpecs[i].concentrationPercent > 0.0f) {
                    atomCountsPerType[i] = static_cast<int>(std::round(totalAtomCount * atomSpecs[i].concentrationPercent / 100.0f));
                }
            }
            // Убеждаемся, что сумма равна totalAtomCount (корректируем последний тип)
            int sum = 0;
            for (int count : atomCountsPerType) {
                sum += count;
            }
            if (sum != totalAtomCount) {
                for (size_t i = atomSpecs.size(); i > 0; --i) {
                    if (atomSpecs[i - 1].concentrationPercent > 0.0f) {
                        atomCountsPerType[i - 1] += (totalAtomCount - sum);
                        break;
                    }
                }
            }
        }
        else {
            // Используем абсолютные значения
            int reservedAtoms = 0;
            for (size_t i = 0; i < atomSpecs.size(); ++i) {
                atomCountsPerType[i] = atomSpecs[i].absoluteCount;
                reservedAtoms += atomSpecs[i].absoluteCount;
            }
            // Если сумма абсолютных значений меньше totalAtomCount, распределяем остаток
            if (reservedAtoms < totalAtomCount) {
                int remainder = totalAtomCount - reservedAtoms;
                for (size_t i = 0; i < atomSpecs.size() && remainder > 0; ++i) {
                    int toAdd = (remainder + atomSpecs.size() - i - 1) / (atomSpecs.size() - i);
                    atomCountsPerType[i] += toAdd;
                    remainder -= toAdd;
                }
            }
        }

        // Вычисляем размер симуляционного ящика на основе totalAtomCount
        const float clampedDensity = std::clamp(density, 0.25f, 3.0f);
        const double effectiveSpacing = spacing / static_cast<double>(clampedDensity);

        const int sideCount = is3d ? std::max(1, static_cast<int>(std::ceil(std::cbrt(static_cast<double>(totalAtomCount)))))
                                   : std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(totalAtomCount)))));

        const double span = sideCount * effectiveSpacing + 2.0 * margin;
        sim.setSizeBox(Vec3f(span, span, is3d ? span : 6));

        // Создаем газ для каждого типа атома
        uint32_t currentSeed = detail::resolveSeed(seed);
        for (size_t i = 0; i < atomSpecs.size(); ++i) {
            if (atomCountsPerType[i] > 0) {
                randomGasInCurrentBox(sim, atomCountsPerType[i], atomSpecs[i].type, is3d, 4.0f, speedScale, 20, currentSeed);
                // Меняем seed для каждого типа атома для хорошего распределения
                currentSeed = (currentSeed != 0) ? currentSeed + 1 : 1;
            }
        }
    }

    void randomGasByConcentration(Simulation& sim, int totalAtomCount, const std::vector<AtomTypeSpec>& concentrations, bool is3d,
                                  double spacing, double margin, float density, float speedScale, uint32_t seed) {
        // Просто передаем в randomGasMixed с концентрациями
        randomGasMixed(sim, totalAtomCount, concentrations, is3d, spacing, margin, density, speedScale, seed);
    }

}
