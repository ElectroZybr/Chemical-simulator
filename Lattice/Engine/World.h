#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/metrics/EnergyMetrics.h"
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/ForceField.h"
#include "Engine/physics/Integrator.h"

// GPU-режим (резидентная физика), opt-in через World::setGpuMode. Forward-decl:
// полный тип нужен только в World.cpp (конструирование/уничтожение/шаг), а
// заголовок виден всему App/Rendering — тянуть туда webgpu не нужно.
class GpuResidentPhysics;

class World {
public:
    explicit World(glm::vec3 size, glm::vec3 renderOffset = glm::vec3{0.0f, 0.0f, 0.0f});
    // Out-of-line: unique_ptr<GpuResidentPhysics> требует полного типа в точке
    // уничтожения, а в заголовке он forward-declared. World держит резидентный GPU
    // (некопируемый/неперемещаемый ресурс VRAM), поэтому World тоже non-copyable
    // и move-only с out-of-line move (как раньше Simulation::WorldState).
    ~World();
    World(World&&) noexcept;
    World& operator=(World&&) noexcept;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void clear();
    void reset();
    void resizeBox(const glm::vec3& newSize, float cellSize = -1.0f);

    void setWorldSize(const glm::vec3& newSize) {
        size = newSize;
        grid.resize(size);
    }
    const glm::vec3& getWorldSize() const noexcept { return size; }

    void setRenderOffset(const glm::vec3& offset) noexcept { renderOffset = offset; }
    const glm::vec3& getRenderOffset() const noexcept { return renderOffset; }

    void setGravity(const glm::vec3& g) {
        gravity = g;
        notifySceneEdited(); // gravity в VRAM устарела — нужен re-upload (wall-kernel читает её как СИЛУ)
    }
    const glm::vec3& getGravity() const noexcept { return gravity; }

    void setGridCellSize(float newSize) { grid.resize(size, newSize); }
    float getGridCellSize() const noexcept { return grid.cellSize; }
    void setNeighborListCutoff(float cutoff) {
        neighborList_.setCutoff(cutoff);
        notifySceneEdited(); // меняет listRadius/cutoffSqr — GPU должен перезалить uniforms
    }
    float getNeighborListCutoff() const noexcept { return neighborList_.cutoff(); }
    void setNeighborListSkin(float skin) {
        neighborList_.setSkin(skin);
        notifySceneEdited();
    }
    float getNeighborListSkin() const noexcept { return neighborList_.skin(); }
    float getNeighborListRadius() const noexcept { return neighborList_.listRadius(); }

    bool isLJEnabled() const { return ljEnabled; }
    void setLJEnabled(bool v) {
        ljEnabled = v;
        notifySceneEdited(); // GPU-шаг диспатчит LJ под флагом ljEnabled_, снятым на upload — рантайм-смена требует re-upload
    }
    bool isCoulombEnabled() const { return coulombEnabled; }
    void setCoulombEnabled(bool v) {
        coulombEnabled = v;
        notifySceneEdited(); // GPU-шаг диспатчит Coulomb под флагом coulombEnabled_, снятым на upload — рантайм-смена требует re-upload
    }
    bool isCoulombLongRangeEnabled() const { return longRangeForcesEnabled; }
    void setCoulombLongRangeEnabled(bool v) { longRangeForcesEnabled = v; }

    AtomStorage& getAtomStorage() noexcept { return atomStorage_; }
    const AtomStorage& getAtomStorage() const noexcept { return atomStorage_; }

    Bond::List& getBonds() noexcept { return bonds_; }
    const Bond::List& getBonds() const noexcept { return bonds_; }

    SpatialGrid& getGrid() noexcept { return grid; }
    const SpatialGrid& getGrid() const noexcept { return grid; }

    NeighborList& getNeighborList() noexcept { return neighborList_; }
    const NeighborList& getNeighborList() const noexcept { return neighborList_; }

    void addAtom(const glm::vec3& start_coords, const glm::vec3& start_speed, AtomData::Type type, bool fixed);
    void addBond(size_t aIndex, size_t bIndex);
    void removeAtom(size_t atomIndex);
    void remapAtomIndices(std::span<const uint32_t> oldToNew);
    void clearAtoms() { atomStorage_.clear(); };
    void clearBonds() { bonds_.clear(); }
    void reserveAtoms(size_t count) { atomStorage_.reserve(count); }
    void appendAtomFast(const glm::vec3& startCoords, const glm::vec3& startSpeed, AtomData::Type type, bool fixed = false) {
        // Под активным GPU подтянуть свежие позиции ДО вставки: addAtom вставляет
        // mobile-атом в середину массива (swap для инварианта "mobile первыми"),
        // поэтому слепое слияние префикса на upload перезатёрло бы новый атом.
        // Синк один раз на батч (первый append снимает dirty), дальше — no-op.
        syncGpuBeforeEdit();
        atomStorage_.addAtom(startCoords, startSpeed, type, fixed);
        invalidateMetrics();
    }
    void finalizeAtomBatch();

    void setTitle(std::string_view title) { title_ = title; }
    const std::string& title() const noexcept { return title_; }
    void setDescription(std::string_view description) { description_ = description; }
    const std::string& description() const noexcept { return description_; }
    void clearMetadata() {
        title_.clear();
        description_.clear();
    }

    struct WorldState {
        Integrator integrator;
        ForceField forceField_;
        float Dt = 0.01f;
        size_t sim_step = 0;
        float sim_time_ns = 0.0f;
        bool bondFormationEnabled_ = false;
        mutable bool metricsCacheValid_ = false;
        mutable EnergyMetrics::Snapshot metricsCache_{};
    };

    WorldState& getState() noexcept { return state_; }
    const WorldState& getState() const noexcept { return state_; }

    // === Управление состоянием симуляции ===
    void update();

    // Параметры интегратора
    void setDt(float dt) noexcept { state_.Dt = dt; }
    float getDt() const noexcept { return state_.Dt; }

    Integrator& getIntegrator() noexcept { return state_.integrator; }
    const Integrator& getIntegrator() const noexcept { return state_.integrator; }

    ForceField& getForceField() noexcept { return state_.forceField_; }
    const ForceField& getForceField() const noexcept { return state_.forceField_; }

    // Параметры связей между атомами
    void setBondFormationEnabled(bool enabled) noexcept { state_.bondFormationEnabled_ = enabled; }
    bool isBondFormationEnabled() const noexcept { return state_.bondFormationEnabled_; }

    // Метрики и статистика
    void invalidateMetrics() const noexcept { state_.metricsCacheValid_ = false; }
    const EnergyMetrics::Snapshot& getMetrics() const;

    // Состояние симуляции
    size_t getSimStep() const noexcept { return state_.sim_step; }
    float getSimTimeNs() const noexcept { return state_.sim_time_ns; }
    void setSimStep(size_t step) noexcept { state_.sim_step = step; }
    void setSimTimeNs(float timeNs) noexcept { state_.sim_time_ns = timeNs; }
    void restoreRuntimeState(size_t step, float timeNs) noexcept {
        state_.sim_step = step;
        state_.sim_time_ns = timeNs;
    }
    void resetRuntimeState() noexcept { restoreRuntimeState(0, 0.0f); }

    // ============================================================================
    // CPU/GPU тумблер для физики ЭТОГО мира. GPU-режим — резидентная физика на GPU:
    // LJ + Coulomb + soft-wall + gravity + силы связей (Morse + угловые) по СТАТИЧНОЙ
    // топологии. В GPU-режиме отключено только образование/разрыв связей (связи
    // существуют на момент входа в режим и неизменны, пока он активен). CPU-путь
    // остаётся дефолтным и нетронутым; переключение в любую сторону синхронизирует
    // состояние через AtomStorage. Требует инициализированного WGPUContext (есть
    // после старта рендера).
    //
    // В новой архитектуре (upstream-рефактор) per-world шаг живёт в World::update(),
    // поэтому резидентный GPU и его жизненный цикл переехали из Simulation::WorldState
    // СЮДА, в World. Simulation остаётся тонким фасадом, делегирующим эти вызовы
    // активному (или всем) миру (см. Simulation::setGpuMode и т.д.).
    // ============================================================================
    void setGpuMode(bool enable);
    [[nodiscard]] bool isGpuMode() const noexcept { return static_cast<bool>(gpu_); }

    // Скачивает позиции/скорости из VRAM в AtomStorage, если активен GPU-режим и
    // CPU-копия устарела. Вызывается перед рендером/метриками/сохранением — редкие
    // точки синхронизации, не каждый физический шаг. В CPU-режиме no-op.
    void syncFromGpuIfNeeded();

    // Пере-биннинг CPU SpatialGrid из текущих CPU-позиций — ТОЛЬКО для
    // диагностических потребителей (визуализация сетки, overlay соседей, debug-
    // панель), которые читают CPU-грид. После 2d hot loop в GPU-режиме перестраивает
    // NL целиком на GPU и CPU-грид НЕ трогает, поэтому он застывает на снимке времени
    // входа в GPU-режим/правки сцены, а атомы в VRAM уезжают — «сетка отделяется от
    // частиц». Этот метод чинит ИМЕННО грид-биннинг, без перестройки CPU NeighborList.
    // PRECONDITION: позиции уже синканы из VRAM (syncFromGpuIfNeeded). В CPU-режиме
    // no-op (грид и так перестраивается физическим шагом перед каждым рендером).
    void refreshDiagnosticsGrid();

    // Сообщить резидентному GPU, что CPU-сцена изменена вне обычного шага (правка
    // скоростей/позиций тулом, resize, cutoff, add/remove): инкремент версии сцены
    // заставит ближайший update залить CPU-сцену в VRAM заново, даже если число
    // атомов не изменилось. В CPU-режиме просто бамп счётчика.
    void notifySceneEdited() noexcept { ++cpuSceneVersion_; }
    // Перед правкой CPU-сцены при активном GPU подтянуть актуальное состояние из VRAM,
    // иначе правка ляжет поверх устаревшего снимка и re-upload откатит прогресс GPU с
    // последнего синка. В CPU-режиме no-op.
    void syncGpuBeforeEdit();

    // Read-only тест-seam (2e): резидентная GPU-физика этого мира, либо nullptr вне
    // GPU-режима. Нужен бенч-гейтам, чтобы читать РЕЗИДЕНТНЫЕ NL-буфера (те, что
    // читает LJ-ядро) через GpuResidentPhysics::readbackNl*. После 2d именно они —
    // авторитетный NL в hot loop, а CPU neighborList() легитимно устаревает. Также —
    // render-bind seam: рендер рисует ВСЕ миры, каждому GPU-миру нужен render-bind его
    // резидентных pos/vel.
    [[nodiscard]] const GpuResidentPhysics* gpuResident() const noexcept { return gpu_.get(); }

    // Диагностика async disp-check (для бенч-матрицы): сколько disp-check'ов забрано
    // async без столла vs ушло в блокирующий backstop. Нули если GPU-режим выключен.
    struct GpuDispCounts {
        uint64_t begins = 0;
        uint64_t consumes = 0;
        uint64_t backstops = 0;
    };
    [[nodiscard]] GpuDispCounts gpuDispCounts() const noexcept;

private:
    // GPU-шаг этого мира (резидентный путь). Вызывается из update() когда gpu_ != null.
    void updateGpu();
    // Заливает текущую CPU-сцену в резидентный GPU (NL Full + полный upload + bonds).
    // Общий путь для входа в GPU-режим и для пере-синка после правки сцены.
    void uploadSceneToGpu();

    glm::vec3 size;
    glm::vec3 renderOffset;
    glm::vec3 gravity;

    bool ljEnabled = true;
    bool coulombEnabled = true;
    // Форк-дефолт false (апстрим: true). Octree long-range Coulomb — аппроксимация Барнса-Хата:
    // не сохраняет импульс точно (наши ConservationTest падали на ~20 при дефолт-on) и расходится
    // с золотым baseline. Оставлен opt-in через setCoulombLongRangeEnabled; GPU-путь его не делает.
    bool longRangeForcesEnabled = false;

    AtomStorage atomStorage_;
    SpatialGrid grid;
    NeighborList neighborList_;
    Bond::List bonds_;
    std::string title_;
    std::string description_;
    WorldState state_;

    // GPU-режим (opt-in). gpu_ == nullptr → CPU-путь.
    std::unique_ptr<GpuResidentPhysics> gpu_;
    bool cpuPositionsDirty_ = false; // VRAM новее AtomStorage, нужен download
    int stepsSinceDispCheck_ = 0;    // шагов с момента ПОСЛЕДНЕГО запуска (kick) async disp-check
    // Версия CPU-сцены против версии, залитой в VRAM. Расхождение → re-upload.
    // Ловит правки контента при неизменном числе атомов (скорости тулом, load той
    // же длины, resize, cutoff), которые проверка по size пропускает.
    uint64_t cpuSceneVersion_ = 0;
    uint64_t gpuUploadedSceneVersion_ = 0;
};
