#include "Simulation.h"

#include <algorithm>
#include <stdexcept>

#include "Engine/io/SimulationStateIO.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"

namespace Lattice {

Simulation::Simulation() = default;

Simulation::WorldId Simulation::createWorld(Vec3f size, Vec3f renderOffset) {
    worlds_.emplace_back(size, renderOffset);
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

void Simulation::setSizeBox(Vec3f newSize, int cellSize) {
    world().resizeBox(newSize, static_cast<float>(cellSize));
}

void Simulation::createAtom(Vec3f start_coords, Vec3f start_speed, AtomData::Type type, bool fixed) {
    world().addAtom(start_coords, start_speed, type, fixed);
}

void Simulation::removeAtom(size_t atomIndex) {
    world().removeAtom(atomIndex);
}

void Simulation::addBond(size_t aIndex, size_t bIndex) { world().addBond(aIndex, bIndex); }

void Simulation::clear() { world().reset(); }

void Simulation::startXYZRecording(std::string_view outputPath) { xyzRecording_.start(outputPath); }

void Simulation::stopXYZRecording() { xyzRecording_.stop(); }

void Simulation::setXYZRecordingStepInterval(uint32_t stepInterval) {
    xyzRecording_.setConfig({.stepInterval = stepInterval});
}

bool Simulation::isXYZRecording() const noexcept { return xyzRecording_.isRecording(); }

// === GPU-режим: фасад над per-world резидентной физикой (живёт в World) ===

void Simulation::syncFromGpuIfNeeded() {
    // По ВСЕМ мирам: updateAll() шагает каждый мир, неактивный GPU-мир тоже
    // продвигается в VRAM, его CPU-копию надо скачать для рендера/метрик.
    for (auto& w : worlds_) {
        w.syncFromGpuIfNeeded();
    }
}

void Simulation::refreshDiagnosticsGrid() {
    // По ВСЕМ мирам (рендер рисует и неактивные); World::refreshDiagnosticsGrid сам
    // пропускает CPU-миры. Precondition «позиции синканы» обеспечивает вызывающий
    // (syncFromGpuIfNeeded в начале кадра).
    for (auto& w : worlds_) {
        w.refreshDiagnosticsGrid();
    }
}

const GpuResidentPhysics* Simulation::gpuResidentAt(WorldId worldId) const {
    if (worldId >= worlds_.size()) {
        throw std::out_of_range("Simulation::gpuResidentAt: invalid world id");
    }
    return worlds_[worldId].gpuResident(); // nullptr если мир в CPU-режиме
}
}
