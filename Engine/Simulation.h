#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/World.h"
#include "Engine/math/Vec3.h"
#include "Engine/metrics/EnergyMetrics.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/ForceField.h"
#include "Engine/physics/Integrator.h"

class Simulation {
public:
    using WorldId = size_t;

    Simulation();

    void update();
    bool updateWorld(WorldId worldId);
    void updateAll();
    void setSizeBox(Vec3f newSize, int cellSize = -1);

    WorldId createWorld(Vec3f size, Vec3f renderOffset = Vec3f{0.0f, 0.0f, 0.0f});
    bool removeWorld(WorldId worldId);
    bool setActiveWorld(WorldId worldId);
    [[nodiscard]] WorldId activeWorldId() const noexcept { return activeWorldIndex_; }
    [[nodiscard]] size_t worldCount() const noexcept { return worlds_.size(); }
    [[nodiscard]] World& worldAt(WorldId worldId);
    [[nodiscard]] const World& worldAt(WorldId worldId) const;

    void createAtom(Vec3f start_coords, Vec3f start_speed, AtomData::Type type, bool fixed = false);
    void removeAtom(size_t atomIndex);
    void addBond(size_t aIndex, size_t bIndex);

    void setDt(float dt) { activeState().Dt = dt; }
    float getDt() const { return activeState().Dt; }
    void setIntegrator(Integrator::Scheme scheme) { activeState().integrator.setScheme(scheme); }
    Integrator::Scheme getIntegrator() const { return activeState().integrator.getScheme(); }
    void setMaxParticleSpeed(float maxSpeed) { activeState().integrator.setMaxParticleSpeed(maxSpeed); }
    float getMaxParticleSpeed() const { return activeState().integrator.maxParticleSpeed(); }
    void setAccelDamping(float accelDamping) { activeState().integrator.setAccelDamping(accelDamping); }
    float getAccelDamping() const { return activeState().integrator.accelDamping(); }

    size_t getSimStep() const { return activeState().sim_step; }
    float simTimeNs() const { return activeState().sim_time_ns; }
    void restoreRuntimeState(int simStep, float simTimeNs) {
        activeState().sim_step = simStep;
        activeState().sim_time_ns = simTimeNs;
    }
    void setWorldTitle(std::string_view title) { world().worldTitle_ = title; }
    const std::string& worldTitle() const { return world().worldTitle_; }
    void setWorldDescription(std::string_view description) { world().worldDescription_ = description; }
    const std::string& worldDescription() const { return world().worldDescription_; }

    float averageKineticEnergyEv() const {
        refreshMetricsCache();
        return activeState().metricsCache_.averageKineticEnergyEv;
    }

    float averagePotentialEnergyEv() const {
        refreshMetricsCache();
        return activeState().metricsCache_.averagePotentialEnergyEv;
    }

    float fullAverageEnergyEv() const {
        refreshMetricsCache();
        return activeState().metricsCache_.fullAverageEnergyEv();
    }

    float fullEnegryPJ() const { return fullAverageEnergyEv() * world().getAtomStorage().size() * Units::kEvToPJ; }

    float temperatureK() const {
        refreshMetricsCache();
        return activeState().metricsCache_.temperatureK();
    }

    float temperatureC() const {
        refreshMetricsCache();
        return activeState().metricsCache_.temperatureC();
    }

    float averageSpeedKmPerHour() const {
        refreshMetricsCache();
        return activeState().metricsCache_.averageSpeedKmPerHour();
    }

    void setBondFormationEnabled(bool enabled) { activeState().bondFormationEnabled_ = enabled; }
    bool isBondFormationEnabled() const { return activeState().bondFormationEnabled_; }
    void setLJEnabled(bool enabled) { world().setLJEnabled(enabled); }
    bool isLJEnabled() const { return world().isLJEnabled(); }
    void setCoulombEnabled(bool enabled) { world().setCoulombEnabled(enabled); }
    bool isCoulombEnabled() const { return world().isCoulombEnabled(); }
    void setGravity(const Vec3f& gravity) { world().setGravity(gravity); }
    Vec3f getGravity() const { return world().getGravity(); }
    void setNeighborListCutoff(float cutoff) { world().getNeighborList().setCutoff(cutoff); }
    float getNeighborListCutoff() const { return world().getNeighborList().cutoff(); }
    void setNeighborListSkin(float skin) { world().getNeighborList().setSkin(skin); }
    float getNeighborListSkin() const { return world().getNeighborList().skin(); }
    float getNeighborListRadius() const { return world().getNeighborList().listRadius(); }

    AtomStorage& atoms() {
        invalidateMetricsCache();
        return world().getAtomStorage();
    }
    const AtomStorage& atoms() const { return world().getAtomStorage(); }
    World& world() { return activeState().world; }
    const World& world() const { return activeState().world; }
    ForceField& forceField() { return activeState().forceField_; }
    const ForceField& forceField() const { return activeState().forceField_; }
    NeighborList& neighborList() { return world().getNeighborList(); }
    const NeighborList& neighborList() const { return world().getNeighborList(); }
    Bond::List& bonds() { return world().getBonds(); }
    const Bond::List& bonds() const { return world().getBonds(); }

    // методы для быстрого создания большого количества атомов
    void reserveAtoms(size_t count) { world().getAtomStorage().reserve(count); }
    void appendAtomFast(Vec3f startCoords, Vec3f startSpeed, AtomData::Type type, bool fixed = false) {
        world().getAtomStorage().addAtom(startCoords, startSpeed, type, fixed);
        invalidateMetricsCache();
    }
    void finalizeAtomBatch() {
        world().getGrid().rebuild(world().getAtomStorage().xDataSpan(), world().getAtomStorage().yDataSpan(),
                                  world().getAtomStorage().zDataSpan());
        world().getNeighborList().clear();
    }
    void clear();

private:
    friend class SimulationStateIO;

    struct WorldState {
        explicit WorldState(Vec3f size, Vec3f renderOffset) : world(size, renderOffset) { world.getNeighborList().setParams(5.f, 1.f); }

        World world;
        Integrator integrator;
        ForceField forceField_;
        float Dt = 0.01f;
        size_t sim_step = 0;
        float sim_time_ns = 0.0f;
        bool bondFormationEnabled_ = false;
        mutable bool metricsCacheValid_ = false;
        mutable EnergyMetrics::Snapshot metricsCache_{};
    };

    StepData makeStepData();
    StepData makeStepData(WorldState& state);
    void updateState(WorldState& state);
    WorldState& activeState();
    const WorldState& activeState() const;
    void invalidateMetricsCache() const { activeState().metricsCacheValid_ = false; }
    void refreshMetricsCache() const;

    std::vector<std::unique_ptr<WorldState>> worlds_;
    WorldId activeWorldIndex_ = 0;
};
