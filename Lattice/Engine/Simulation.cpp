#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Lattice/Engine/Consts.h"
#include "Lattice/Engine/io/SimulationStateIO.h"
#include "Lattice/Engine/io/MoleculeMol.hpp"
#include "Lattice/Engine/metrics/Profiler.h"
#include "Lattice/Engine/physics/BondOps.h"
#include "Lattice/Engine/physics/MoleculeOps.hpp"

namespace Lattice {
namespace {

constexpr float kManualSpawnBorderMargin = 1.0f;

bool isInsideManualSpawnBounds(const World& world, const glm::vec3& position) {
    const glm::vec3 worldSize = world.getWorldSize();
    return kManualSpawnBorderMargin <= position.x && position.x <= worldSize.x - kManualSpawnBorderMargin &&
           kManualSpawnBorderMargin <= position.y && position.y <= worldSize.y - kManualSpawnBorderMargin &&
           kManualSpawnBorderMargin <= position.z && position.z <= worldSize.z - kManualSpawnBorderMargin;
}

bool collidesWithExistingAtoms(const AtomStorage& storage, const glm::vec3& position, float radius) {
    for (size_t atomIndex = 0; atomIndex < storage.size(); ++atomIndex) {
        const float otherRadius = AtomData::getProps(storage.type()[atomIndex]).radius;
        const float minDistance = 2.0f * (radius + otherRadius);
        const glm::vec3 delta = storage.pos(atomIndex) - position;
        if (glm::dot(delta, delta) <= minDistance * minDistance) {
            return true;
        }
    }
    return false;
}

void removeAtomIndicesDescending(Simulation& simulation, std::vector<size_t>& atomIndices) {
    if (atomIndices.empty()) {
        return;
    }

    std::sort(atomIndices.begin(), atomIndices.end());
    atomIndices.erase(std::unique(atomIndices.begin(), atomIndices.end()), atomIndices.end());
    simulation.removeAtoms(std::move(atomIndices));
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string normalizeAtomSymbol(std::string value) {
    value = lowercase(std::move(value));
    if (!value.empty()) {
        value[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[0])));
    }
    return value;
}

} // namespace

Simulation::Simulation() {
    chemistryData.init();
};

glm::mat3 Simulation::randomRotationMatrix() {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());

    const glm::quat qx = glm::angleAxis(angleDist(rng), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat qy = glm::angleAxis(angleDist(rng), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat qz = glm::angleAxis(angleDist(rng), glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::mat3_cast(qz * qy * qx);
}

void Simulation::finishAtomBatch() {
    if (!atomBatchActive_) {
        world().finalizeAtomBatch();
        return;
    }

    atomBatchActive_ = false;
    if (atomBatchDirty_) {
        world().finalizeAtomBatch();
        atomBatchDirty_ = false;
    }
}

Simulation::WorldId Simulation::createWorld(glm::vec3 size, glm::vec3 renderOffset) {
    worlds_.emplace_back(size, renderOffset);
    const WorldId worldId = worlds_.size() - 1;
    if (worlds_.size() == 1) {
        activeWorldIndex_ = worldId;
    }
    worlds_.back().setChemistryData(chemistryData);
    return worldId;
}

bool Simulation::removeWorld(WorldId worldId) {
    if (worldId >= worlds_.size() || worlds_.size() <= 1) {
        return false;
    }

    worlds_.erase(worlds_.begin() + static_cast<std::ptrdiff_t>(worldId));
    if (activeWorldIndex_ == worldId) {
        activeWorldIndex_ = std::min(worldId, worlds_.size() - 1);
    }
    else if (activeWorldIndex_ > worldId) {
        --activeWorldIndex_;
    }
    return true;
}

bool Simulation::setActiveWorld(WorldId worldId) {
    if (worldId >= worlds_.size()) {
        return false;
    }
    activeWorldIndex_ = worldId;
    return true;
}

World& Simulation::worldAt(WorldId worldId) {
    if (worldId >= worlds_.size()) {
        throw std::out_of_range("Simulation::worldAt: invalid world id");
    }
    return worlds_[worldId];
}

const World& Simulation::worldAt(WorldId worldId) const {
    if (worldId >= worlds_.size()) {
        throw std::out_of_range("Simulation::worldAt: invalid world id");
    }
    return worlds_[worldId];
}

World& Simulation::world() {
    if (worlds_.empty() || activeWorldIndex_ >= worlds_.size()) {
        throw std::runtime_error("Simulation: no active world");
    }
    return worlds_[activeWorldIndex_];
}

const World& Simulation::world() const {
    if (worlds_.empty() || activeWorldIndex_ >= worlds_.size()) {
        throw std::runtime_error("Simulation: no active world");
    }
    return worlds_[activeWorldIndex_];
}

void Simulation::update() {
    PROFILE_SCOPE("Simulation::update");
    world().update();
    xyzRecording_.onSimulationStep(*this);
}

void Simulation::updateWorld(WorldId worldId) {
    PROFILE_SCOPE("Simulation::updateWorld");

    if (worldId < worlds_.size()) {
        worlds_[worldId].update();
        if (worldId == activeWorldIndex_) {
            xyzRecording_.onSimulationStep(*this);
        }
    }
}

void Simulation::updateAll() {
    PROFILE_SCOPE("Simulation::updateAll");
    for (auto& w : worlds_) {
        w.update();
    }
    xyzRecording_.onSimulationStep(*this);
}

void Simulation::setSizeBox(glm::vec3 newSize, int cellSize) {
    world().resizeBox(newSize, static_cast<float>(cellSize));
}

void Simulation::createAtom(glm::vec3 start_coords, glm::vec3 start_speed, AtomData::Type type, bool fixed) {
    world().addAtom(start_coords, start_speed, type, fixed);
}

void Simulation::removeAtom(size_t atomIndex) {
    world().removeAtom(atomIndex);
}

void Simulation::removeAtoms(std::vector<size_t> atomIndices) {
    world().removeAtoms(std::move(atomIndices));
}

void Simulation::addBond(size_t aIndex, size_t bIndex, uint8_t order) { world().addBond(aIndex, bIndex, order, chemistryData); }

bool Simulation::loadMoleculeTemplate(std::string name, const std::filesystem::path& moleculePath) {
    if (name.empty()) {
        return false;
    }

    moleculeTemplates_[name] = MoleculeMol::loadTemplate(moleculePath);
    MoleculeOps::calcMoleculeHybridization(moleculeTemplates_.at(name), chemistryData);
    return true;
}

bool Simulation::hasMoleculeTemplate(std::string_view name) const {
    return findMoleculeTemplate(name) != nullptr;
}

const MoleculeTemplate* Simulation::findMoleculeTemplate(std::string_view name) const {
    const auto it = moleculeTemplates_.find(std::string(name));
    if (it == moleculeTemplates_.end()) {
        return nullptr;
    }
    return &it->second;
}

bool Simulation::canSpawnMolecule(std::string_view speciesName, glm::vec3 start_coords, const std::optional<glm::mat3>& rotation) const {
    if (!isInsideManualSpawnBounds(world(), start_coords)) {
        return false;
    }

    const AtomStorage& storage = atoms();
    const std::string moleculeKey = lowercase(std::string(speciesName));
    if (const MoleculeTemplate* molecule = findMoleculeTemplate(moleculeKey); molecule != nullptr) {
        if (molecule->atoms.empty()) {
            return false;
        }

        const glm::mat3 actualRotation = rotation.value_or(glm::mat3(1.0f));
        for (const MoleculeAtom& atom : molecule->atoms) {
            const glm::vec3 worldPos = start_coords + actualRotation * atom.localPos;
            const float atomRadius = AtomData::getProps(atom.type).radius;
            if (!isInsideManualSpawnBounds(world(), worldPos) || collidesWithExistingAtoms(storage, worldPos, atomRadius)) {
                return false;
            }
        }
        return true;
    }

    const std::string atomSymbol = normalizeAtomSymbol(std::string(speciesName));
    for (size_t i = 0; i < static_cast<size_t>(AtomData::Type::COUNT); ++i) {
        const AtomData::Type type = static_cast<AtomData::Type>(i);
        if (AtomData::symbol(type) != atomSymbol) {
            continue;
        }

        const float atomRadius = AtomData::getProps(type).radius;
        return !collidesWithExistingAtoms(storage, start_coords, atomRadius);
    }

    return false;
}

bool Simulation::spawnMoleculeChecked(std::string_view speciesName, glm::vec3 start_coords, const std::optional<glm::mat3>& rotation, bool fixed) {
    if (!canSpawnMolecule(speciesName, start_coords, rotation)) {
        return false;
    }
    return spawnMolecule(speciesName, start_coords, rotation, fixed);
}

glm::vec3 Simulation::temperatureVelocity(std::string_view speciesName, float temperature, bool is3d, glm::vec3 fallbackVelocity) const {
    if (temperature <= 0.0f) {
        return fallbackVelocity;
    }

    float mass = 0.0f;
    const std::string moleculeKey = lowercase(std::string(speciesName));
    if (const MoleculeTemplate* molecule = findMoleculeTemplate(moleculeKey); molecule != nullptr) {
        for (const MoleculeAtom& atom : molecule->atoms) {
            mass += AtomData::getProps(atom.type).mass;
        }
    } else {
        const std::string atomSymbol = normalizeAtomSymbol(std::string(speciesName));
        for (size_t i = 0; i < static_cast<size_t>(AtomData::Type::COUNT); ++i) {
            const AtomData::Type type = static_cast<AtomData::Type>(i);
            if (AtomData::symbol(type) == atomSymbol) {
                mass = AtomData::getProps(type).mass;
                break;
            }
        }
    }

    if (mass <= 0.0f) {
        return fallbackVelocity;
    }

    static thread_local std::mt19937 thermalRng(std::random_device{}());
    const float sigma = std::sqrt(Units::kboltzmann * temperature / mass);
    std::normal_distribution<float> maxwell(0.0f, sigma);
    return glm::vec3(
        maxwell(thermalRng),
        maxwell(thermalRng),
        is3d ? maxwell(thermalRng) : 0.0f
    );
}

bool Simulation::spawnMolecule(std::string_view speciesName, glm::vec3 start_coords, const std::optional<glm::mat3>& rotation, bool fixed) {
    const glm::vec3 velocity = temperatureVelocity(speciesName, 0.0f, true, glm::vec3(0.0f));
    const std::string moleculeKey = lowercase(std::string(speciesName));
    if (const MoleculeTemplate* molecule = findMoleculeTemplate(moleculeKey); molecule != nullptr) {
        if (molecule->atoms.empty()) {
            return false;
        }

        const glm::mat3 actualRotation = rotation.value_or(glm::mat3(1.0f));

        std::vector<AtomStorage::AtomId> createdIds;
        createdIds.reserve(molecule->atoms.size());
        reserveAtoms(atoms().size() + molecule->atoms.size());

        for (const MoleculeAtom& atom : molecule->atoms) {
            const glm::vec3 worldPos = start_coords + actualRotation * atom.localPos;
            createdIds.push_back(appendAtomFast(worldPos, velocity, atom.type, fixed, atom.hybridization));
        }
        if (atomBatchActive_) {
            atomBatchDirty_ = true;
        } else {
            world().finalizeAtomBatch();
        }

        const AtomStorage& storage = atoms();
        std::vector<size_t> createdIndices;
        createdIndices.reserve(createdIds.size());
        for (const AtomStorage::AtomId atomId : createdIds) {
            const size_t atomIndex = storage.indexOf(atomId);
            if (atomIndex >= storage.size()) {
                return false;
            }
            createdIndices.push_back(atomIndex);
        }

        for (const MoleculeBond& bond : molecule->bonds) {
            if (bond.atomA < createdIndices.size() && bond.atomB < createdIndices.size()) {
                addBond(createdIndices[bond.atomA], createdIndices[bond.atomB], bond.order);
            }
        }

        return true;
    }

    const std::string atomSymbol = normalizeAtomSymbol(std::string(speciesName));
    for (size_t i = 0; i < static_cast<size_t>(AtomData::Type::COUNT); ++i) {
        const AtomData::Type type = static_cast<AtomData::Type>(i);
        if (AtomData::symbol(type) != atomSymbol) {
            continue;
        }

        (void)appendAtomFast(start_coords, velocity, type, fixed);
        if (atomBatchActive_) {
            atomBatchDirty_ = true;
        } else {
            world().finalizeAtomBatch();
        }
        return true;
    }

    return false;
}

bool Simulation::randomSpawn(std::string_view speciesName, const SpawnOptions& options) {
    const glm::vec3 regionMin = options.min;
    const glm::vec3 regionMax = (options.max == glm::vec3(0.0f)) ? world().getWorldSize() : options.max;
    const bool is3d = std::abs(regionMax.z - regionMin.z) > 1e-5f;
    const std::string moleculeKey = lowercase(std::string(speciesName));
    if (const MoleculeTemplate* molecule = findMoleculeTemplate(moleculeKey); molecule != nullptr) {
        const AtomStorage& storage = atoms();
        const float minDistanceSqr = options.minDistance * options.minDistance;
        static thread_local std::mt19937 rng(std::random_device{}());

        for (uint32_t attempt = 0; attempt < options.maxAttempts; ++attempt) {
            const glm::mat3 rotation = options.randomRotation ? randomRotationMatrix() : glm::mat3(1.0f);
            glm::vec3 minOffset(std::numeric_limits<float>::max());
            glm::vec3 maxOffset(std::numeric_limits<float>::lowest());
            for (const MoleculeAtom& atom : molecule->atoms) {
                const glm::vec3 rotated = rotation * atom.localPos;
                minOffset = glm::min(minOffset, rotated);
                maxOffset = glm::max(maxOffset, rotated);
            }

            const auto sampleAxis = [&](float axisMin, float axisMax, float minAtomOffset, float maxAtomOffset) -> std::optional<float> {
                const float lower = axisMin + options.margin - minAtomOffset;
                const float upper = axisMax - options.margin - maxAtomOffset;

                if (lower > upper) {
                    return std::nullopt;
                }

                if (std::abs(upper - lower) <= 1e-5f) {
                    return lower;
                }

                std::uniform_real_distribution<float> dist(lower, upper);
                return dist(rng);
            };

            const std::optional<float> x = sampleAxis(regionMin.x, regionMax.x, minOffset.x, maxOffset.x);
            const std::optional<float> y = sampleAxis(regionMin.y, regionMax.y, minOffset.y, maxOffset.y);
            if (!x.has_value() || !y.has_value()) {
                return false;
            }

            float z = (regionMin.z + regionMax.z) * 0.5f;
            if (is3d) {
                const std::optional<float> sampledZ = sampleAxis(regionMin.z, regionMax.z, minOffset.z, maxOffset.z);
                if (!sampledZ.has_value()) {
                    return false;
                }
                z = *sampledZ;
            } else {
                const float lower = regionMin.z + options.margin - minOffset.z;
                const float upper = regionMax.z - options.margin - maxOffset.z;
                if (lower > upper) {
                    return false;
                }
                z = std::clamp((regionMin.z + regionMax.z) * 0.5f, lower, upper);
            }

            const glm::vec3 origin(*x, *y, z);

            bool fits = true;
            std::vector<size_t> collidingIndices;
            for (const MoleculeAtom& atom : molecule->atoms) {
                const glm::vec3 pos = origin + rotation * atom.localPos;

                if (pos.x < regionMin.x + options.margin || pos.y < regionMin.y + options.margin || pos.z < regionMin.z + options.margin ||
                    pos.x > regionMax.x - options.margin || pos.y > regionMax.y - options.margin || pos.z > regionMax.z - options.margin) {
                    fits = false;
                    break;
                }

                for (size_t otherIndex = 0; otherIndex < storage.size(); ++otherIndex) {
                    const glm::vec3 delta = pos - storage.pos(otherIndex);
                    if (glm::dot(delta, delta) < minDistanceSqr) {
                        if (options.collisionMode == SpawnCollisionMode::Replace) {
                            if (otherIndex < options.replaceExistingCount) {
                                collidingIndices.push_back(otherIndex);
                            } else {
                                fits = false;
                                break;
                            }
                        } else {
                            fits = false;
                            break;
                        }
                    }
                }

                if (!fits) {
                    break;
                }
            }

            if (fits) {
                if (options.collisionMode == SpawnCollisionMode::Replace) {
                    removeAtomIndicesDescending(*this, collidingIndices);
                }
                const glm::vec3 velocity = temperatureVelocity(speciesName, options.temperature, is3d, options.velocity);
                std::vector<AtomStorage::AtomId> createdIds;
                createdIds.reserve(molecule->atoms.size());
                reserveAtoms(atoms().size() + molecule->atoms.size());

                for (const MoleculeAtom& atom : molecule->atoms) {
                    const glm::vec3 worldPos = origin + rotation * atom.localPos;
                    createdIds.push_back(appendAtomFast(worldPos, velocity, atom.type, options.fixed));
                }
                if (atomBatchActive_) {
                    atomBatchDirty_ = true;
                } else {
                    world().finalizeAtomBatch();
                }

                const AtomStorage& createdStorage = atoms();
                std::vector<size_t> createdIndices;
                createdIndices.reserve(createdIds.size());
                for (const AtomStorage::AtomId atomId : createdIds) {
                    const size_t atomIndex = createdStorage.indexOf(atomId);
                    if (atomIndex >= createdStorage.size()) {
                        return false;
                    }
                    createdIndices.push_back(atomIndex);
                }

                for (const MoleculeBond& bond : molecule->bonds) {
                    if (bond.atomA < createdIndices.size() && bond.atomB < createdIndices.size()) {
                        addBond(createdIndices[bond.atomA], createdIndices[bond.atomB], bond.order);
                    }
                }

                return true;
            }
        }

        return false;
    }

    const std::string atomSymbol = normalizeAtomSymbol(std::string(speciesName));
    for (size_t i = 0; i < static_cast<size_t>(AtomData::Type::COUNT); ++i) {
        const AtomData::Type type = static_cast<AtomData::Type>(i);
        if (AtomData::symbol(type) != atomSymbol) {
            continue;
        }

        const AtomStorage& storage = atoms();
        const float minDistanceSqr = options.minDistance * options.minDistance;
        static thread_local std::mt19937 rng(std::random_device{}());
        const glm::vec3 velocity = temperatureVelocity(speciesName, options.temperature, is3d, options.velocity);

        auto sampleCoord = [&](float minValue, float maxValue) {
            if (maxValue <= minValue) {
                return minValue;
            }
            std::uniform_real_distribution<float> dist(minValue, maxValue);
            return dist(rng);
        };

        for (uint32_t attempt = 0; attempt < options.maxAttempts; ++attempt) {
            const float minX = regionMin.x + options.margin;
            const float maxX = regionMax.x - options.margin;
            const float minY = regionMin.y + options.margin;
            const float maxY = regionMax.y - options.margin;
            const float minZ = regionMin.z + options.margin;
            const float maxZ = regionMax.z - options.margin;

            const glm::vec3 pos(
                sampleCoord(minX, maxX),
                sampleCoord(minY, maxY),
                is3d ? sampleCoord(minZ, maxZ) : ((regionMin.z + regionMax.z) * 0.5f)
            );

            bool fits = true;
            std::vector<size_t> collidingIndices;
            for (size_t otherIndex = 0; otherIndex < storage.size(); ++otherIndex) {
                const glm::vec3 delta = pos - storage.pos(otherIndex);
                if (glm::dot(delta, delta) < minDistanceSqr) {
                    if (options.collisionMode == SpawnCollisionMode::Replace) {
                        if (otherIndex < options.replaceExistingCount) {
                            collidingIndices.push_back(otherIndex);
                        } else {
                            fits = false;
                            break;
                        }
                    } else {
                        fits = false;
                        break;
                    }
                }
            }

            if (fits) {
                if (options.collisionMode == SpawnCollisionMode::Replace) {
                    removeAtomIndicesDescending(*this, collidingIndices);
                }
                (void)appendAtomFast(pos, velocity, type, options.fixed);
                if (atomBatchActive_) {
                    atomBatchDirty_ = true;
                } else {
                    world().finalizeAtomBatch();
                }
                return true;
            }
        }

        return false;
    }

    return false;
}

float Simulation::lj_min(AtomData::Type a, AtomData::Type b) {
    const float sigma = 0.5f * (AtomData::getProps(a).ljA0 + AtomData::getProps(b).ljA0);
    return sigma * std::pow(2.0f, 1.0f / 6.0f);
}

void Simulation::clear() { world().reset(); }

void Simulation::startXYZRecording(std::string_view outputPath) { xyzRecording_.start(outputPath); }

void Simulation::stopXYZRecording() { xyzRecording_.stop(); }

void Simulation::setXYZRecordingStepInterval(uint32_t stepInterval) {
    xyzRecording_.setConfig({.stepInterval = stepInterval});
}

bool Simulation::isXYZRecording() const noexcept { return xyzRecording_.isRecording(); }
}
