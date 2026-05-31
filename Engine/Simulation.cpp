#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Engine/io/SimulationStateIO.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/Bond.h"

Simulation::Simulation() = default;

Simulation::WorldState& Simulation::activeState() {
    if (worlds_.empty() || activeWorldIndex_ >= worlds_.size()) {
        throw std::runtime_error("Simulation: no active world");
    }
    return *worlds_[activeWorldIndex_];
}

const Simulation::WorldState& Simulation::activeState() const {
    if (worlds_.empty() || activeWorldIndex_ >= worlds_.size()) {
        throw std::runtime_error("Simulation: no active world");
    }
    return *worlds_[activeWorldIndex_];
}

Simulation::WorldId Simulation::createWorld(Vec3f size, Vec3f renderOffset) {
    worlds_.push_back(std::make_unique<WorldState>(size, renderOffset));
    const WorldId worldId = worlds_.size() - 1;
    if (worlds_.size() == 1) {
        activeWorldIndex_ = worldId;
    }
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
    return worlds_[worldId]->world;
}

const World& Simulation::worldAt(WorldId worldId) const {
    if (worldId >= worlds_.size()) {
        throw std::out_of_range("Simulation::worldAt: invalid world id");
    }
    return worlds_[worldId]->world;
}

void Simulation::refreshMetricsCache() const {
    const WorldState& state = activeState();
    if (state.metricsCacheValid_) {
        return;
    }

    state.metricsCache_ = EnergyMetrics::buildSnapshot(state.world.getAtomStorage());
    state.metricsCacheValid_ = true;
}

StepData Simulation::makeStepData() {
    return makeStepData(activeState());
}

StepData Simulation::makeStepData(WorldState& state) {
    return StepData{
        .world = state.world,
        .forceField = state.forceField_,
        .neighborList = state.world.getNeighborList(),
        .allowBondFormation = state.bondFormationEnabled_,
        .accelDamping = state.integrator.accelDamping(),
        .dt = state.Dt,
    };
}

void Simulation::update() {
    PROFILE_SCOPE("Simulation::update");
    updateState(activeState());
}

bool Simulation::updateWorld(WorldId worldId) {
    if (worldId >= worlds_.size()) {
        return false;
    }
    updateState(*worlds_[worldId]);
    return true;
}

void Simulation::updateAll() {
    PROFILE_SCOPE("Simulation::updateAll");
    for (const auto& state : worlds_) {
        updateState(*state);
    }
}

void Simulation::updateState(WorldState& state) {
    if (state.world.getNeighborList().needsRebuild(state.world.getAtomStorage())) {
        state.world.getNeighborList().rebuildPipeline(state.world.getAtomStorage(), state.world, state.sim_step);
    }

    StepData stepData = makeStepData(state);
    state.integrator.step(stepData);
    state.metricsCacheValid_ = false;
    ++state.sim_step;
    state.sim_time_ns += state.Dt * Units::kTimeUnitToNs;
}

void Simulation::setSizeBox(Vec3f newSize, int cellSize) {
    World& activeWorld = world();
    activeWorld.setWorldSize(newSize);
    activeWorld.setGridCellSize(cellSize);
    activeWorld.getGrid().rebuild(activeWorld.getAtomStorage().xDataSpan(), activeWorld.getAtomStorage().yDataSpan(),
                                  activeWorld.getAtomStorage().zDataSpan());
}

void Simulation::createAtom(Vec3f start_coords, Vec3f start_speed, AtomData::Type type, bool fixed) {
    world().addAtom(start_coords, start_speed, type, fixed);
    invalidateMetricsCache();
}

void Simulation::removeAtom(size_t atomIndex) {
    world().removeAtom(atomIndex);
    invalidateMetricsCache();
}

void Simulation::addBond(size_t aIndex, size_t bIndex) { Bond::CreateBond(world().getBonds(), aIndex, bIndex, world().getAtomStorage()); }

void Simulation::clear() {
    WorldState& state = activeState();
    state.world.clear();

    state.world.worldTitle_.clear();
    state.world.worldDescription_.clear();

    invalidateMetricsCache();
    state.sim_step = 0;
    state.sim_time_ns = 0.0f;
}
