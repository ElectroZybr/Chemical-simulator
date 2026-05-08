#pragma once

#include <string>

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
    Simulation(World& sim_box);

    void update();
    void setSizeBox(Vec3f newSize, int cellSize = -1);

    bool createAtom(Vec3f start_coords, Vec3f start_speed, AtomData::Type type, bool fixed = false);
    bool removeAtom(size_t atomIndex);
    void addBond(size_t aIndex, size_t bIndex);

    void setDt(float dt) { Dt = dt; }
    float getDt() const { return Dt; }
    void setIntegrator(Integrator::Scheme scheme) { integrator.setScheme(scheme); }
    Integrator::Scheme getIntegrator() const { return integrator.getScheme(); }
    void setMaxParticleSpeed(float maxSpeed) { integrator.setMaxParticleSpeed(maxSpeed); }
    float getMaxParticleSpeed() const { return integrator.maxParticleSpeed(); }
    void setAccelDamping(float accelDamping) { integrator.setAccelDamping(accelDamping); }
    float getAccelDamping() const { return integrator.accelDamping(); }

    size_t getSimStep() const { return sim_step; }
    float simTimeNs() const { return sim_time_ns; }
    void restoreRuntimeState(int simStep, float simTimeNs) {
        sim_step = simStep;
        sim_time_ns = simTimeNs;
    }
    void setWorldTitle(std::string_view title) { world_.worldTitle_ = title; }
    const std::string& worldTitle() const { return world_.worldTitle_; }
    void setWorldDescription(std::string_view description) { world_.worldDescription_ = description; }
    const std::string& worldDescription() const { return world_.worldDescription_; }

    float averageKineticEnergyEv() const {
        refreshMetricsCache();
        return metricsCache_.averageKineticEnergyEv;
    }

    float averagePotentialEnergyEv() const {
        refreshMetricsCache();
        return metricsCache_.averagePotentialEnergyEv;
    }

    float fullAverageEnergyEv() const {
        refreshMetricsCache();
        return metricsCache_.fullAverageEnergyEv();
    }

    float fullEnegryPJ() const { return fullAverageEnergyEv() * world_.getAtomStorage().size() * Units::kEvToPJ; }

    float temperatureK() const {
        refreshMetricsCache();
        return metricsCache_.temperatureK();
    }

    float temperatureC() const {
        refreshMetricsCache();
        return metricsCache_.temperatureC();
    }

    float averageSpeedKmPerHour() const {
        refreshMetricsCache();
        return metricsCache_.averageSpeedKmPerHour();
    }

    void setBondFormationEnabled(bool enabled) { bondFormationEnabled_ = enabled; }
    bool isBondFormationEnabled() const { return bondFormationEnabled_; }
    void setLJEnabled(bool enabled) { world_.setLJEnabled(enabled); }
    bool isLJEnabled() const { return world_.isLJEnabled(); }
    void setCoulombEnabled(bool enabled) { world_.setCoulombEnabled(enabled); }
    bool isCoulombEnabled() const { return world_.isCoulombEnabled(); }
    void setGravity(const Vec3f& gravity) { world_.setGravity(gravity); }
    Vec3f getGravity() const { return world_.getGravity(); }
    void setNeighborListCutoff(float cutoff) { world_.getNeighborList().setCutoff(cutoff); }
    float getNeighborListCutoff() const { return world_.getNeighborList().cutoff(); }
    void setNeighborListSkin(float skin) { world_.getNeighborList().setSkin(skin); }
    float getNeighborListSkin() const { return world_.getNeighborList().skin(); }
    float getNeighborListRadius() const { return world_.getNeighborList().listRadius(); }

    AtomStorage& atoms() {
        invalidateMetricsCache();
        return world_.getAtomStorage();
    }
    const AtomStorage& atoms() const { return world_.getAtomStorage(); }
    World& world() { return world_; }
    const World& world() const { return world_; }
    ForceField& forceField() { return forceField_; }
    const ForceField& forceField() const { return forceField_; }
    NeighborList& neighborList() { return world_.getNeighborList(); }
    const NeighborList& neighborList() const { return world_.getNeighborList(); }
    Bond::List& bonds() { return world_.getBonds(); }
    const Bond::List& bonds() const { return world_.getBonds(); }

    // методы для быстрого создания большого количества атомов
    void reserveAtoms(size_t count) { world_.getAtomStorage().reserve(count); }
    void appendAtomFast(Vec3f startCoords, Vec3f startSpeed, AtomData::Type type, bool fixed = false) {
        world_.getAtomStorage().addAtom(startCoords, startSpeed, type, fixed);
        invalidateMetricsCache();
    }
    void finalizeAtomBatch() {
        world_.getGrid().rebuild(world_.getAtomStorage().xDataSpan(), world_.getAtomStorage().yDataSpan(),
                                 world_.getAtomStorage().zDataSpan());
        world_.getNeighborList().clear();
    }
    void clear();

private:
    friend class SimulationStateIO;
    StepData makeStepData();
    void invalidateMetricsCache() const { metricsCacheValid_ = false; }
    void refreshMetricsCache() const;

    World& world_;
    Integrator integrator;
    ForceField forceField_;
    float Dt = 0.01f;
    size_t sim_step = 0;
    float sim_time_ns = 0.0f;
    bool bondFormationEnabled_ = false;
    mutable bool metricsCacheValid_ = false;
    mutable EnergyMetrics::Snapshot metricsCache_{};
};
