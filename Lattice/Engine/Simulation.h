#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Lattice/Engine/World.h"
#include "Lattice/Engine/io/MoleculeTemplate.h"
#include "Lattice/Engine/io/XYZRecordingSession.h"

namespace Lattice {

enum class SpawnCollisionMode : uint8_t {
    Add,
    Replace,
};

struct SpawnOptions {
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
    float temperature = 0.0f;
    float margin = 2.0f;
    float minDistance = 4.0f;
    uint32_t maxAttempts = 32;
    bool randomRotation = true;
    bool fixed = false;
    SpawnCollisionMode collisionMode = SpawnCollisionMode::Add;
    size_t replaceExistingCount = std::numeric_limits<size_t>::max();
};

class Simulation {
public:
    using WorldId = size_t;

    Simulation();

    /* === Управление мирами === */
    WorldId createWorld(glm::vec3 size, glm::vec3 renderOffset = glm::vec3{0.0f, 0.0f, 0.0f});
    bool removeWorld(WorldId worldId);
    bool setActiveWorld(WorldId worldId);
    
    [[nodiscard]] WorldId activeWorldId() const noexcept { return activeWorldIndex_; }
    [[nodiscard]] size_t worldCount() const noexcept { return worlds_.size(); }
    
    [[nodiscard]] World& worldAt(WorldId worldId);
    [[nodiscard]] const World& worldAt(WorldId worldId) const;

    /* === Обновление симуляции === */
    void update();
    void updateWorld(WorldId worldId);
    void updateAll();

    /* === управление активным миром === */
    [[nodiscard]] World& world();
    [[nodiscard]] const World& world() const;

    void createAtom(glm::vec3 start_coords, glm::vec3 start_speed, AtomData::Type type, bool fixed = false);
    void removeAtom(size_t atomIndex);
    void removeAtoms(std::vector<size_t> atomIndices);
    void addBond(size_t aIndex, size_t bIndex);
    bool loadMoleculeTemplate(std::string name, const std::filesystem::path& moleculePath);
    bool hasMoleculeTemplate(std::string_view name) const;
    [[nodiscard]] const MoleculeTemplate* findMoleculeTemplate(std::string_view name) const;
    [[nodiscard]] const std::unordered_map<std::string, MoleculeTemplate>& moleculeTemplates() const noexcept { return moleculeTemplates_; }
    [[nodiscard]] bool canSpawnMolecule(std::string_view speciesName, glm::vec3 start_coords,
                                        const std::optional<glm::mat3>& rotation = std::nullopt) const;
    [[nodiscard]] bool spawnMoleculeChecked(std::string_view speciesName, glm::vec3 start_coords, const std::optional<glm::mat3>& rotation,
                                            bool fixed);
    [[nodiscard]] bool spawnMolecule(std::string_view speciesName, glm::vec3 start_coords, const std::optional<glm::mat3>& rotation, bool fixed);
    [[nodiscard]] bool randomSpawn(std::string_view speciesName, const SpawnOptions& options = {});
    [[nodiscard]] static glm::mat3 randomRotationMatrix();
    float lj_min(AtomData::Type a, AtomData::Type b);

    void setDt(float dt) { world().setDt(dt); }
    float getDt() const { return world().getDt(); }
    bool setIntegrator(std::string_view id) { return world().getIntegrator().setIntegrator(id); }
    std::string_view getIntegrator() const { return world().getIntegrator().getIntegrator(); }
    void setMaxParticleSpeed(float maxSpeed) { world().getIntegrator().setMaxParticleSpeed(maxSpeed); }
    float getMaxParticleSpeed() const { return world().getIntegrator().maxParticleSpeed(); }
    bool setThermostat(std::string_view id) { return world().getThermostat().setThermostat(id); }
    std::string_view getThermostat() const { return world().getThermostat().getThermostat(); }
    void setThermostatTemperature(float temperature) { world().getThermostat().setTemperature(temperature); }
    float getThermostatTemperature() const { return world().getThermostat().temperature(); }
    void setThermostatParam(float param) { world().getThermostat().setParam(param); }
    float getThermostatParam() const { return world().getThermostat().param(); }

    size_t getSimStep() const { return world().getSimStep(); }
    float simTimeNs() const { return world().getSimTimeNs(); }
    void restoreRuntimeState(size_t simStep, float simTimeNs) { world().restoreRuntimeState(simStep, simTimeNs); }
    void setWorldTitle(std::string_view title) { world().setTitle(title); }
    const std::string& worldTitle() const { return world().title(); }
    void setWorldDescription(std::string_view description) { world().setDescription(description); }
    const std::string& worldDescription() const { return world().description(); }

    float averageKineticEnergyEv() const { return world().getMetrics().averageKineticEnergyEv; }
    float averagePotentialEnergyEv() const { return world().getMetrics().averagePotentialEnergyEv; }
    float fullAverageEnergyEv() const { return world().getMetrics().fullAverageEnergyEv(); }
    float fullEnegryPJ() const { return fullAverageEnergyEv() * atoms().size() * Units::kEvToPJ; }
    float temperatureK() const { return world().getMetrics().temperatureK(); }
    float temperatureC() const { return world().getMetrics().temperatureC(); }
    float averageSpeedKmPerHour() const { return world().getMetrics().averageSpeedKmPerHour(); }

    void setBondFormationEnabled(bool enabled) { world().setBondFormationEnabled(enabled); }
    bool isBondFormationEnabled() const { return world().isBondFormationEnabled(); }
    void setLJEnabled(bool enabled) { world().setLJEnabled(enabled); }
    bool isLJEnabled() const { return world().isLJEnabled(); }
    void setCoulombEnabled(bool enabled) { world().setCoulombEnabled(enabled); }
    bool isCoulombEnabled() const { return world().isCoulombEnabled(); }
    void setGravity(const glm::vec3& gravity) { world().setGravity(gravity); }
    glm::vec3 getGravity() const { return world().getGravity(); }
    void setNeighborListCutoff(float cutoff) { world().setNeighborListCutoff(cutoff); }
    float getNeighborListCutoff() const { return world().getNeighborListCutoff(); }
    void setNeighborListSkin(float skin) { world().setNeighborListSkin(skin); }
    float getNeighborListSkin() const { return world().getNeighborListSkin(); }
    float getNeighborListRadius() const { return world().getNeighborListRadius(); }

    AtomStorage& atoms() {
        world().invalidateMetrics();
        return world().getAtomStorage();
    }
    const AtomStorage& atoms() const { return world().getAtomStorage(); }
    ForceField& forceField() { return world().getForceField(); }
    const ForceField& forceField() const { return world().getForceField(); }
    NeighborList& neighborList() { return world().getNeighborList(); }
    const NeighborList& neighborList() const { return world().getNeighborList(); }
    Bond::List& bonds() { return world().getBonds(); }
    const Bond::List& bonds() const { return world().getBonds(); }

    // Быстрое создание большого количества атомов
    void reserveAtoms(size_t count) { world().reserveAtoms(count); }
    [[nodiscard]] AtomStorage::AtomId appendAtomFast(glm::vec3 startCoords, glm::vec3 startSpeed, AtomData::Type type, bool fixed = false) {
        return world().appendAtomFast(startCoords, startSpeed, type, fixed);
    }
    void beginAtomBatch() {
        atomBatchActive_ = true;
        atomBatchDirty_ = false;
    }
    void finishAtomBatch();

    void setSizeBox(glm::vec3 newSize, int cellSize = -1);
    void clear();
    void startXYZRecording(std::string_view outputPath);
    void stopXYZRecording();
    void setXYZRecordingStepInterval(uint32_t stepInterval);
    [[nodiscard]] bool isXYZRecording() const noexcept;
    [[nodiscard]] uint32_t xyzRecordingStepInterval() const noexcept { return xyzRecording_.stepInterval(); }
    [[nodiscard]] uint64_t xyzFrameCount() const noexcept { return xyzRecording_.frameCount(); }
    [[nodiscard]] float xyzFPS() const noexcept { return xyzRecording_.fps(); }

private:
    friend class SimulationStateIO;
    [[nodiscard]] glm::vec3 temperatureVelocity(std::string_view speciesName, float temperature, bool is3d,glm::vec3 fallbackVelocity = glm::vec3(0.0f)) const;
    std::vector<World> worlds_;
    WorldId activeWorldIndex_ = 0;
    // загруженные шаблоны молекул
    std::unordered_map<std::string, MoleculeTemplate> moleculeTemplates_;
    XYZRecordingSession xyzRecording_;
    bool atomBatchActive_ = false;
    bool atomBatchDirty_ = false;
};
}
