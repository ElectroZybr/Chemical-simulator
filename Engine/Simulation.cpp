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
    if (state.world.getAtomStorage().size() != state.gpu->totalCount() ||
        state.cpuSceneVersion != state.gpuUploadedSceneVersion) {
        uploadSceneToGpu(state);
    }

    // NL-rebuild решается по GPU-редукции смещения. Раньше disp-check блокировал
    // (dev.poll каждые kDispCheckCadence шагов дренил GPU-очередь — главный sync,
    // сериализующий CPU+GPU). Теперь АСИНХРОННО: редукция запускается, результат
    // читается неблокирующе на следующих шагах; rebuild по готовому (на 1-2 шага
    // устаревшему) значению. Hard backstop: pending старше kDispReadbackMaxLagSteps
    // дожидается форс-синком ПЕРЕД шагом — безопасная граница для mobile-mobile пар
    // именно 0.5*skin (два атома идут навстречу), поэтому latency нельзя отпускать.
    constexpr int kDispCheckCadence = 4;
    constexpr int kDispReadbackMaxLagSteps = 2;
    const float skin = state.world.getNeighborList().skin();
    const float threshold = 0.5f * skin;
    const float thresholdSqr = threshold * threshold;

    auto rebuildNeighborList = [&]() {
        // Шаг 2d: NL пересобирается ЦЕЛИКОМ на GPU из резидентных позиций — БЕЗ
        // CPU round-trip (downloadToCpu + CPU rebuildPipeline + uploadNeighborList).
        // Именно этот round-trip был perf-корнем «CPU отъедает у GPU». Параметры
        // сетки берём из CPU SpatialGrid (он не перестраивается в hot loop, но его
        // size/cellSize/countCells валидны с момента конфигурации/входа в GPU-режим
        // и совпадают с биннингом, который ждёт GPU-builder). refPos обновляется
        // внутри rebuildNeighborListOnGpu (база disp-проверки) — pending уже снят
        // выше. CPU-копия NL при этом легитимно устаревает: её контент в GPU hot
        // loop никто не читает (LJ-силы и интегрирование идут по резидентным
        // буферам), а перед правкой сцены uploadSceneToGpu строит NL на CPU заново.
        const SpatialGrid& grid = state.world.getGrid();
        const float lr = state.world.getNeighborList().listRadius();
        state.gpu->rebuildNeighborListOnGpu(grid.size.x, grid.size.y, grid.size.z, grid.cellSize,
                                            static_cast<uint32_t>(grid.countCells), lr * lr);
    };

    if (auto disp = state.gpu->tryConsumeMaxDisplacementSqr(); disp.has_value()) {
        // Готовый результат забран без столла.
        if (*disp > thresholdSqr) {
            rebuildNeighborList();
        }
    }
    else if (state.gpu->dispCheckPending() && state.gpu->dispCheckAgeSteps() >= kDispReadbackMaxLagSteps) {
        // Backstop: дольше лимита нельзя ждать — дождаться сейчас, до шага со старым NL.
        if (state.gpu->finishMaxDisplacementSqrBlocking() > thresholdSqr) {
            rebuildNeighborList();
        }
    }
    if (!state.gpu->dispCheckPending() && state.stepsSinceDispCheck >= kDispCheckCadence) {
        // Нет pending и каденция истекла — запустить новую async-редукцию.
        state.stepsSinceDispCheck = 0;
        state.gpu->beginMaxDisplacementSqrAsync();
    }

    state.gpu->step(state.Dt, state.integrator.accelDamping());
    ++state.stepsSinceDispCheck;
    state.cpuPositionsDirty = true; // VRAM новее CPU-копии
    state.metricsCacheValid_ = false;
    ++state.sim_step;
    state.sim_time_ns += state.Dt * Units::kTimeUnitToNs;
}

void Simulation::uploadSceneToGpu(WorldState& state) {
    // Любой pending async disp-check считался против старого refPos — после
    // перезаливки он устареет, а его буфер сейчас mid-map. Безопасно снести.
    state.gpu->discardPendingDisplacementCheck();

    // Если GPU ушёл вперёд (CPU-копия устарела), сперва слить его прогресс в CPU,
    // иначе полный upload откатил бы существующие атомы к последнему CPU-снимку.
    // Усечённая выгрузка (downloadToCpu clamp) трогает только старый префикс —
    // свежедобавленный хвост остаётся из CPU. Это покрывает структурные правки,
    // которые лишь бампят версию без собственного синка (finalizeAtomBatch,
    // cutoff/skin). clear() заранее сбрасывает dirty, поэтому сброшенная сцена тут
    // не «воскресает». In-place правки (термо-тул, resize-сдвиг) синкаются ДО
    // правки сами, так что здесь их dirty уже снят и слияние их не затирает.
    if (state.cpuPositionsDirty) {
        state.gpu->downloadToCpu(state.world.getAtomStorage(), /*withVelocities=*/true);
        state.cpuPositionsDirty = false;
    }

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
    state.gpuUploadedSceneVersion = state.cpuSceneVersion; // VRAM теперь соответствует CPU-сцене
}

void Simulation::notifySceneEdited() { ++activeState().cpuSceneVersion; }

void Simulation::syncGpuBeforeEdit() {
    WorldState& state = activeState();
    if (state.gpu && state.cpuPositionsDirty) {
        state.gpu->downloadToCpu(state.world.getAtomStorage(), /*withVelocities=*/true);
        state.cpuPositionsDirty = false;
    }
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
        state.gpu->discardPendingDisplacementCheck(); // буфер не должен остаться mid-map при сносе
        state.gpu->downloadToCpu(state.world.getAtomStorage(), /*withVelocities=*/true);
        state.gpu.reset();
        state.cpuPositionsDirty = false;
        state.world.getNeighborList().clear();
    }
    state.metricsCacheValid_ = false;
}

bool Simulation::isGpuMode() const { return static_cast<bool>(activeState().gpu); }

Simulation::GpuDispCounts Simulation::activeGpuDispCounts() const {
    const WorldState& state = activeState();
    if (!state.gpu) {
        return {};
    }
    return {state.gpu->dispBeginCount(), state.gpu->dispConsumeCount(), state.gpu->dispBackstopCount()};
}

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

void Simulation::refreshDiagnosticsGrid() {
    // Рендер рисует ВСЕ миры (updateAll шагает каждый), и виз-сетка/overlay/stats
    // читают grid КАЖДОГО GPU-мира — поэтому перебинниваем грид всех GPU-миров, а не
    // только активного (тот же контракт «все миры», что и syncFromGpuIfNeeded). Иначе
    // неактивный GPU-мир рисовал бы замороженную сетку при drawGrid.
    //
    // Только биннинг грида из УЖЕ синканных CPU-позиций (precondition: вызывающий
    // сделал syncFromGpuIfNeeded в начале кадра). НЕ качаем VRAM повторно и НЕ трогаем
    // CPU NeighborList — viz/overlay/stats читают именно грид, а NL-readback на каждый
    // кадр был бы дорог (это диагностика, не hot loop).
    for (const auto& state : worlds_) {
        if (!state->gpu) {
            continue; // CPU-мир: грид перестраивается физическим шагом, уже свежий.
        }
        World& world = state->world;
        AtomStorage& atoms = world.getAtomStorage();
        world.getGrid().rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());
    }
}

void Simulation::setSizeBox(Vec3f newSize, int cellSize) {
    syncGpuBeforeEdit(); // grid rebuild ниже должен видеть актуальные позиции
    World& activeWorld = world();
    activeWorld.setWorldSize(newSize);
    activeWorld.setGridCellSize(cellSize);
    activeWorld.getGrid().rebuild(activeWorld.getAtomStorage().xDataSpan(), activeWorld.getAtomStorage().yDataSpan(),
                                  activeWorld.getAtomStorage().zDataSpan());
    notifySceneEdited(); // worldMax_ в VRAM устарел — нужен re-upload
}

void Simulation::createAtom(Vec3f start_coords, Vec3f start_speed, AtomData::Type type, bool fixed) {
    syncGpuBeforeEdit(); // подтянуть свежие позиции существующих атомов перед добавлением
    world().addAtom(start_coords, start_speed, type, fixed);
    invalidateMetricsCache();
    notifySceneEdited();
}

void Simulation::removeAtom(size_t atomIndex) {
    syncGpuBeforeEdit();
    world().removeAtom(atomIndex);
    invalidateMetricsCache();
    notifySceneEdited();
}

void Simulation::addBond(size_t aIndex, size_t bIndex) { Bond::CreateBond(world().getBonds(), aIndex, bIndex, world().getAtomStorage()); }

void Simulation::clear() {
    WorldState& state = activeState();
    // Сцена сбрасывается целиком — старое состояние VRAM не нужно (и качать его
    // перед очисткой нельзя: затёрло бы свежезагружаемые атомы). Версия растёт,
    // чтобы пустая/новая сцена была залита в GPU на ближайшем шаге.
    state.cpuPositionsDirty = false;
    state.world.clear();

    state.world.worldTitle_.clear();
    state.world.worldDescription_.clear();

    invalidateMetricsCache();
    state.sim_step = 0;
    state.sim_time_ns = 0.0f;
    ++state.cpuSceneVersion;
}
