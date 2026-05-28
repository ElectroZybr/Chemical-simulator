#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Engine/io/SimulationStateIO.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"

Simulation::WorldState::WorldState(Vec3f size, Vec3f renderOffset) : world(size, renderOffset) {
    world.getNeighborList().setParams(5.f, 1.f);
#ifdef LATTICELAB_USE_TBB
    // Auto-mode: на каждом rebuild NL выбирает Half для mobileCount<5000
    // (избегаем 2x работу force loop на маленьких сценах) и Full выше
    // (parallel-выгода окупает удвоенную NL).
    world.getNeighborList().setAutoMode(5000);
#endif
}

Simulation::WorldState::~WorldState() = default;

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
    if (state.gpu) {
        updateStateGpu(state);
        return;
    }

    if (state.world.getNeighborList().needsRebuild(state.world.getAtomStorage())) {
        state.world.getNeighborList().rebuildPipeline(state.world.getAtomStorage(), state.world, state.sim_step);
    }

    StepData stepData = makeStepData(state);
    state.integrator.step(stepData);
    state.metricsCacheValid_ = false;
    ++state.sim_step;
    state.sim_time_ns += state.Dt * Units::kTimeUnitToNs;
}

void Simulation::updateStateGpu(WorldState& state) {
    // Сцена могла измениться под резидентным GPU (добавили/удалили атомы,
    // загрузили сцену): VRAM-буфера залиты под старый totalCount — путь NL
    // rebuild переполнил бы их (writeBuffer overrun), а новые атомы вообще не
    // интегрировались бы. Замечаем расхождение по счётчику атомов и заново
    // заливаем активную сцену (буфера растут в ensureCapacity). CPU-копия при
    // этом свежая: рендер делает syncFromGpuIfNeeded в начале кадра, до
    // UI-обработчиков правки сцены.
    if (state.world.getAtomStorage().size() != state.gpu->totalCount()) {
        uploadSceneToGpu(state);
    }

    // NL-rebuild решается по GPU-редукции смещения, проверяемой не каждый шаг
    // (poll — это sync), а батчем раз в kDispCheckCadence шагов. Кадэнс должен
    // быть меньше интервала реального rebuild, чтобы не пропустить (skin=1 даёт
    // запас: пара видна пока атом не уехал на skin от reference).
    constexpr int kDispCheckCadence = 4;
    if (state.stepsSinceDispCheck >= kDispCheckCadence) {
        state.stepsSinceDispCheck = 0;
        const float skin = state.world.getNeighborList().skin();
        const float threshold = 0.5f * skin;
        if (state.gpu->maxDisplacementSqr() > threshold * threshold) {
            // Реальный rebuild: скачиваем позиции на CPU, перестраиваем grid+NL
            // на CPU (канонический путь), заливаем новый NL обратно в VRAM.
            state.gpu->downloadToCpu(state.world.getAtomStorage(), /*withVelocities=*/true);
            state.cpuPositionsDirty = false;
            state.world.getNeighborList().rebuildPipeline(state.world.getAtomStorage(), state.world, state.sim_step);
            state.gpu->uploadNeighborList(state.world.getNeighborList());
        }
    }

    state.gpu->step(state.Dt, state.integrator.accelDamping());
    ++state.stepsSinceDispCheck;
    state.cpuPositionsDirty = true; // VRAM новее CPU-копии
    state.metricsCacheValid_ = false;
    ++state.sim_step;
    state.sim_time_ns += state.Dt * Units::kTimeUnitToNs;
}

void Simulation::uploadSceneToGpu(WorldState& state) {
    // NL в Full (требование резидентного LJ-пути), свежий build, полная заливка
    // позиций/скоростей/типов/NL в VRAM. ensureCapacity растит буфера под
    // текущий размер сцены — поэтому путь годится и для входа в GPU-режим, и для
    // пере-синка после правки сцены (add/remove/load).
    NeighborList& nl = state.world.getNeighborList();
    nl.setMode(NeighborListMode::Full);
    nl.rebuildPipeline(state.world.getAtomStorage(), state.world, static_cast<int>(state.sim_step));

    const Vec3f size = state.world.getWorldSize();
    state.gpu->uploadFromCpu(state.world.getAtomStorage(), nl, state.forceField_.ljForceField(),
                             static_cast<float>(size.x), static_cast<float>(size.y), static_cast<float>(size.z));
    state.cpuPositionsDirty = false;
    state.stepsSinceDispCheck = 0;
}

void Simulation::setGpuMode(bool enable) {
    WorldState& state = activeState();
    if (enable == static_cast<bool>(state.gpu)) {
        return; // уже в нужном режиме
    }

    if (enable) {
        // CPU -> GPU: создаём резидентность и заливаем активную сцену.
        state.gpu = std::make_unique<GpuResidentPhysics>();
        uploadSceneToGpu(state);
    }
    else {
        // GPU -> CPU: вернуть актуальное состояние в AtomStorage, снести GPU,
        // пометить NL на перестройку (позиции изменились).
        state.gpu->downloadToCpu(state.world.getAtomStorage(), /*withVelocities=*/true);
        state.gpu.reset();
        state.cpuPositionsDirty = false;
        state.world.getNeighborList().clear();
    }
    state.metricsCacheValid_ = false;
}

bool Simulation::isGpuMode() const { return static_cast<bool>(activeState().gpu); }

void Simulation::syncFromGpuIfNeeded() {
    // Синкаем ВСЕ GPU-миры, а не только активный: updateAll() шагает каждый мир,
    // поэтому неактивный GPU-мир тоже продвигается в VRAM, и его CPU-копию надо
    // скачать для рендера/метрик. Иначе неактивный GPU-мир "застывает" на экране
    // (рендер читает устаревшую CPU-копию), хотя физика на GPU идёт.
    for (const auto& state : worlds_) {
        if (state->gpu && state->cpuPositionsDirty) {
            state->gpu->downloadToCpu(state->world.getAtomStorage(), /*withVelocities=*/true);
            state->cpuPositionsDirty = false;
            state->metricsCacheValid_ = false;
        }
    }
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
