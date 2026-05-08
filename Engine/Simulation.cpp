#include "Simulation.h"

#include <cmath>

#include "Engine/io/SimulationStateIO.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/Bond.h"

Simulation::Simulation(World& world) : world_(world), integrator() { world_.getNeighborList().setParams(5.f, 1.f); }

void Simulation::refreshMetricsCache() const {
    if (metricsCacheValid_) {
        return;
    }

    metricsCache_ = EnergyMetrics::buildSnapshot(world_.getAtomStorage());
    metricsCacheValid_ = true;
}

StepData Simulation::makeStepData() {
    return StepData{
        .world = world_,
        .forceField = forceField_,
        .neighborList = world_.getNeighborList(),
        .allowBondFormation = bondFormationEnabled_,
        .accelDamping = integrator.accelDamping(),
        .dt = Dt,
    };
}

void Simulation::update() {
    PROFILE_SCOPE("Simulation::update");
    if (world_.getNeighborList().needsRebuild(world_.getAtomStorage())) {
        world_.getNeighborList().rebuildPipeline(world_.getAtomStorage(), world_, sim_step);
    }

    StepData stepData = makeStepData();
    integrator.step(stepData);
    invalidateMetricsCache();
    ++sim_step;
    sim_time_ns += Dt * Units::kTimeUnitToNs;
}

void Simulation::setSizeBox(Vec3f newSize, int cellSize) {
    world_.setWorldSize(newSize);
    world_.setGridCellSize(cellSize);
    world_.getGrid().rebuild(world_.getAtomStorage().xDataSpan(), world_.getAtomStorage().yDataSpan(), world_.getAtomStorage().zDataSpan());
}

void Simulation::createAtom(Vec3f start_coords, Vec3f start_speed, AtomData::Type type, bool fixed) {
    world_.addAtom(start_coords, start_speed, type, fixed);
    invalidateMetricsCache();
}

void Simulation::removeAtom(size_t atomIndex) {
    world_.removeAtom(atomIndex);
    invalidateMetricsCache();
}

void Simulation::addBond(size_t aIndex, size_t bIndex) { Bond::CreateBond(world_.getBonds(), aIndex, bIndex, world_.getAtomStorage()); }

void Simulation::clear() {
    world_.clear();

    world_.worldTitle_.clear();
    world_.worldDescription_.clear();

    invalidateMetricsCache();
    sim_step = 0;
    sim_time_ns = 0.0f;
}
