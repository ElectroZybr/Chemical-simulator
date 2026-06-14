#include "Gas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>
#include <vector>

namespace Generators {
    namespace detail {
        constexpr float kGasDensity3DMin = 0.001f;
        constexpr float kGasDensity3DMax = 0.010f;
        constexpr float kGasDensity2DMin = 0.005f;
        constexpr float kGasDensity2DMax = 0.060f;
        constexpr float kGasMinDistance = 4.0f;
        constexpr float kGasFlatBoxDepth = 6.0f;

        inline float randomUnit() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }

        inline glm::vec3 randomVelocity(float scale) {
            return glm::vec3(randomUnit() * 2.0f - 1.0f, randomUnit() * 2.0f - 1.0f, randomUnit() * 2.0f - 1.0f) * scale;
        }

        bool hasNeighborInStorage(const Lattice::Simulation& sim, const glm::vec3& coords, float delta) {
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
                            const glm::vec3 deltaVec = coords - atoms.pos(atomIndex);
                            if (glm::dot(deltaVec, deltaVec) < deltaSqr) {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }

        uint32_t resolveSeed(uint32_t seed) { return seed == 0 ? std::random_device{}() : seed; }

        float clampGasDensity(float density, bool is3d) {
            return std::clamp(density, is3d ? kGasDensity3DMin : kGasDensity2DMin, is3d ? kGasDensity3DMax : kGasDensity2DMax);
        }

        double minimumGasSpan(int atomCount, bool is3d, double margin) {
            const int sideCount = is3d ? std::max(1, static_cast<int>(std::ceil(std::cbrt(static_cast<double>(atomCount)))))
                                       : std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(atomCount)))));
            return static_cast<double>(sideCount) * static_cast<double>(kGasMinDistance) + 2.0 * margin;
        }

        double densityDrivenGasSpan(int atomCount, bool is3d, float density) {
            const double safeDensity = std::max(static_cast<double>(density), 1e-9);
            if (is3d) {
                return std::cbrt(static_cast<double>(atomCount) / safeDensity);
            }
            return std::sqrt(static_cast<double>(atomCount) / safeDensity);
        }
    }

    int randomGasInCurrentBox(Lattice::Simulation& sim, int atomCount, AtomData::Type type, bool is3d, float minDistance, float speedScale,
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
        std::vector<glm::vec3> acceptedPositions;
        acceptedPositions.reserve(static_cast<size_t>(atomCount));

        std::vector<std::vector<glm::vec3>> pendingByCell(static_cast<size_t>(world.getGrid().countCells));
        const int pendingRadiusCells = std::max(1, static_cast<int>(std::ceil(minDistance / static_cast<float>(world.getGrid().cellSize))));

        const auto isTooCloseToPending = [&](const glm::vec3& coords) {
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
                        for (const glm::vec3& other : bucket) {
                            const glm::vec3 deltaVec = coords - other;
                            if (glm::dot(deltaVec, deltaVec) < minDistanceSqr) {
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
                const glm::vec3 coords(rx + 2.0, ry + 2.0, is3d ? (rz + 2.0) : zMid);

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
        for (const glm::vec3& pos : acceptedPositions) {
            const glm::vec3 randomSpeed = detail::randomVelocity(speedScale);
            sim.appendAtomFast(pos, is3d ? randomSpeed : glm::vec3(randomSpeed.x, randomSpeed.y, 0.0f), type);
        }

        sim.finalizeAtomBatch();
        return static_cast<int>(acceptedPositions.size());
    }

    void randomGas(Lattice::Simulation& sim, int atomCount, AtomData::Type type, bool is3d, double spacing, double margin, float density,
                   float speedScale, uint32_t seed) {
        (void)spacing;
        atomCount = std::max(0, atomCount);
        const float clampedDensity = detail::clampGasDensity(density, is3d);
        const double targetSpan = detail::densityDrivenGasSpan(atomCount, is3d, clampedDensity);
        const double span = std::max(targetSpan, detail::minimumGasSpan(atomCount, is3d, margin));

        sim.setSizeBox(glm::vec3(span, span, is3d ? span : detail::kGasFlatBoxDepth));

        randomGasInCurrentBox(sim, atomCount, type, is3d, detail::kGasMinDistance, speedScale, 20, seed);
    }

    void randomGasMixed(Lattice::Simulation& sim, int totalAtomCount, const std::vector<AtomTypeSpec>& atomSpecs, bool is3d, double spacing,
                        double margin, float density, float speedScale, uint32_t seed) {
        (void)spacing;
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
        const float clampedDensity = detail::clampGasDensity(density, is3d);
        const double targetSpan = detail::densityDrivenGasSpan(totalAtomCount, is3d, clampedDensity);
        const double span = std::max(targetSpan, detail::minimumGasSpan(totalAtomCount, is3d, margin));
        sim.setSizeBox(glm::vec3(span, span, is3d ? span : detail::kGasFlatBoxDepth));

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

    void randomGasByConcentration(Lattice::Simulation& sim, int totalAtomCount, const std::vector<AtomTypeSpec>& concentrations, bool is3d,
                                  double spacing, double margin, float density, float speedScale, uint32_t seed) {
        // Просто передаем в randomGasMixed с концентрациями
        randomGasMixed(sim, totalAtomCount, concentrations, is3d, spacing, margin, density, speedScale, seed);
    }
}
