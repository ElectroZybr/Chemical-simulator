#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/World.h"
#include "Engine/io/XYZRecordingSession.h"

namespace Lattice {

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
    void addBond(size_t aIndex, size_t bIndex);

    void setDt(float dt) { world().setDt(dt); }
    float getDt() const { return world().getDt(); }
    void setIntegrator(Integrator::Scheme scheme) { world().getIntegrator().setScheme(scheme); }
    Integrator::Scheme getIntegrator() const { return world().getIntegrator().getScheme(); }
    void setMaxParticleSpeed(float maxSpeed) { world().getIntegrator().setMaxParticleSpeed(maxSpeed); }
    float getMaxParticleSpeed() const { return world().getIntegrator().maxParticleSpeed(); }
    void setAccelDamping(float accelDamping) { world().getIntegrator().setAccelDamping(accelDamping); }
    float getAccelDamping() const { return world().getIntegrator().accelDamping(); }
    void setAndersenTemperature(float temperature) { world().getIntegrator().setAndersenTemperature(temperature); }
    float getAndersenTemperature() const { return world().getIntegrator().andersenTemperature(); }

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
    void appendAtomFast(glm::vec3 startCoords, glm::vec3 startSpeed, AtomData::Type type, bool fixed = false) {
        world().appendAtomFast(startCoords, startSpeed, type, fixed);
    }
    void finalizeAtomBatch() { world().finalizeAtomBatch(); }

    void setBoxSize(glm::vec3 newSize, int cellSize = -1);
    void setTargetBoxSize(glm::vec3 newSize, int cellSize = -1);
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

    std::vector<World> worlds_;
    WorldId activeWorldIndex_ = 0;
    XYZRecordingSession xyzRecording_;
};
}
