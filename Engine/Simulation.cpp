#include "Simulation.h"

#include <cmath>

#include "Engine/io/SimulationStateIO.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/Bond.h"

Simulation::Simulation(World& world) : world_(world), integrator() {
    world_.getNeighborList().setParams(5.f, 1.f);
    forceField_.syncWalls(world_);
}

void Simulation::refreshMetricsCache() const {
    if (metricsCacheValid_) {
        return;
    }

    metricsCache_ = EnergyMetrics::buildSnapshot(world_.getAtomStorage());
    metricsCacheValid_ = true;
}

StepData Simulation::makeStepData() {
    return StepData{
        .atomStorage = world_.getAtomStorage(),
        .bonds = world_.getBonds(),
        .box = world_,
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

    forceField_.syncWalls(world_);
    world_.getGrid().rebuild(world_.getAtomStorage().xDataSpan(), world_.getAtomStorage().yDataSpan(), world_.getAtomStorage().zDataSpan());
    world_.getNeighborList().clear();
}

bool Simulation::createAtom(Vec3f start_coords, Vec3f start_speed, AtomData::Type type, bool fixed) {
    world_.getAtomStorage().addAtom(start_coords, start_speed, type, fixed);
    invalidateMetricsCache();
    world_.getGrid().rebuild(world_.getAtomStorage().xDataSpan(), world_.getAtomStorage().yDataSpan(), world_.getAtomStorage().zDataSpan());
    return true;
}

bool Simulation::removeAtom(size_t atomIndex) {
    if (atomIndex >= world_.getAtomStorage().size()) {
        return false;
    }

    const size_t lastIndex = world_.getAtomStorage().size() - 1;

    for (auto it = world_.getBonds().begin(); it != world_.getBonds().end();) {
        if (it->aIndex == atomIndex || it->bIndex == atomIndex) {
            if (it->aIndex == atomIndex && it->bIndex != atomIndex && it->bIndex < world_.getAtomStorage().size()) {
                ++world_.getAtomStorage().valenceCount(it->bIndex);
            }
            if (it->bIndex == atomIndex && it->aIndex != atomIndex && it->aIndex < world_.getAtomStorage().size()) {
                ++world_.getAtomStorage().valenceCount(it->aIndex);
            }
            it = world_.getBonds().erase(it);
            continue;
        }

        if (atomIndex != lastIndex) {
            if (it->aIndex == lastIndex) {
                it->aIndex = atomIndex;
            }
            if (it->bIndex == lastIndex) {
                it->bIndex = atomIndex;
            }
        }

        ++it;
    }

    world_.getAtomStorage().removeAtom(atomIndex);
    invalidateMetricsCache();
    world_.getGrid().rebuild(world_.getAtomStorage().xDataSpan(), world_.getAtomStorage().yDataSpan(), world_.getAtomStorage().zDataSpan());
    return true;
}

void Simulation::addBond(size_t aIndex, size_t bIndex) {
    if (aIndex >= world_.getAtomStorage().size() || bIndex >= world_.getAtomStorage().size()) {
        return;
    }

    Bond::CreateBond(world_.getBonds(), aIndex, bIndex, world_.getAtomStorage());
}

void Simulation::clear() {
    world_.getAtomStorage().clear();
    invalidateMetricsCache();
    world_.getBonds().clear();
    world_.worldTitle_.clear();
    world_.worldDescription_.clear();
    world_.getGrid().rebuild(world_.getAtomStorage().xDataSpan(), world_.getAtomStorage().yDataSpan(), world_.getAtomStorage().zDataSpan());
    world_.getNeighborList().clear();
    sim_step = 0;
    sim_time_ns = 0.0f;
}
