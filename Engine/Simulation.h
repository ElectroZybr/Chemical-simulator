#pragma once

#include <cstdint>
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

class GpuResidentPhysics; // GPU-режим (резидентная физика), opt-in через setGpuMode

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

    // CPU/GPU тумблер для физики активного мира. GPU-режим — резидентная физика
    // на GPU: LJ + Coulomb + soft-wall + gravity + силы связей (Morse + угловые) по
    // СТАТИЧНОЙ топологии. В GPU-режиме отключено только образование/разрыв связей
    // (связи существуют на момент входа в режим и неизменны, пока он активен).
    // CPU-путь остаётся дефолтным и нетронутым; переключение в любую сторону
    // синхронизирует состояние через AtomStorage. Требует инициализированного
    // WGPUContext (есть после старта рендера).
    void setGpuMode(bool enable);
    [[nodiscard]] bool isGpuMode() const;
    // Скачивает позиции/скорости из VRAM в AtomStorage, если активен GPU-режим
    // и CPU-копия устарела. Вызывается перед рендером/метриками/сохранением —
    // редкие точки синхронизации, не каждый физический шаг. В CPU-режиме no-op.
    void syncFromGpuIfNeeded();

    // Пере-биннинг CPU SpatialGrid ВСЕХ GPU-миров (не только активного: рендер
    // рисует и неактивные миры) из текущих CPU-позиций — ТОЛЬКО для диагностических
    // потребителей (визуализация сетки, overlay соседей, debug-панель), которые
    // читают CPU-грид; CPU-миры пропускаются. После 2d hot loop в GPU-режиме
    // перестраивает NL целиком на GPU и CPU-грид НЕ трогает, поэтому он застывает
    // на снимке времени входа в GPU-режим/правки сцены, а атомы в VRAM уезжают —
    // «сетка отделяется от частиц». Этот метод чинит ИМЕННО грид-биннинг (то, что
    // читают viz/overlay/stats), без перестройки CPU NeighborList.
    //
    // PRECONDITION: позиции уже синканы из VRAM (syncFromGpuIfNeeded в начале
    // кадра) — метод НЕ качает повторно, только перебиннивает уже свежий CPU-снимок.
    // Стоимость O(N) на каденции рендера (~60fps), а НЕ на каденции физики — это
    // не тот CPU round-trip, что убрала 2d из hot loop. В CPU-режиме no-op (грид и
    // так перестраивается физическим шагом перед каждым рендером).
    void refreshDiagnosticsGrid();

    // Диагностика async disp-check активного GPU-мира (для бенч-матрицы): сколько
    // disp-check'ов забрано async без столла vs ушло в блокирующий backstop. Доля
    // backstop≈1 => текущая каденция сабмита глушит async (rapid-submit), ≈0 =>
    // async работает (app-like per-frame pacing). Нули если GPU-режим выключен.
    struct GpuDispCounts {
        uint64_t begins = 0;
        uint64_t consumes = 0;
        uint64_t backstops = 0;
    };
    [[nodiscard]] GpuDispCounts activeGpuDispCounts() const;

    // Read-only тест-seam (2e): резидентная GPU-физика активного мира, либо nullptr
    // вне GPU-режима. Нужен бенч-гейтам, чтобы читать РЕЗИДЕНТНЫЕ NL-буфера (те, что
    // читает LJ-ядро) через GpuResidentPhysics::readbackNl*. После 2d именно они —
    // авторитетный NL в hot loop, а CPU neighborList() легитимно устаревает.
    [[nodiscard]] const GpuResidentPhysics* activeGpuResident() const { return activeState().gpu.get(); }

    // Read-only render-bind seam: резидентная GPU-физика мира worldId, либо nullptr
    // если этот мир в CPU-режиме. Обобщает activeGpuResident() с активного на ЛЮБОЙ
    // мир — рендер рисует ВСЕ миры (drawShot цикл по worldId), и каждому GPU-миру
    // нужен render-bind его резидентных pos/vel. Bounds-check как worldAt(WorldId).
    [[nodiscard]] const GpuResidentPhysics* gpuResidentAt(WorldId worldId) const;

    // Сообщить резидентному GPU, что CPU-сцена изменена вне обычного шага (правка
    // скоростей/позиций тулом, resize, cutoff, add/remove): инкремент версии
    // сцены заставит ближайший updateState залить CPU-сцену в VRAM заново, даже
    // если число атомов не изменилось. В CPU-режиме просто бамп счётчика.
    void notifySceneEdited();
    // Перед правкой CPU-сцены при активном GPU подтянуть актуальное состояние из
    // VRAM, иначе правка ляжет поверх устаревшего снимка и re-upload откатит
    // прогресс GPU с последнего синка. В CPU-режиме no-op.
    void syncGpuBeforeEdit();

    void setLJEnabled(bool enabled) {
        world().setLJEnabled(enabled);
        notifySceneEdited(); // GPU-шаг диспатчит LJ под флагом ljEnabled_, снятым на upload — рантайм-смена требует re-upload (иначе toggle не долетит до GPU)
    }
    bool isLJEnabled() const { return world().isLJEnabled(); }
    void setCoulombEnabled(bool enabled) {
        world().setCoulombEnabled(enabled);
        notifySceneEdited(); // GPU-шаг диспатчит Coulomb под флагом coulombEnabled_, снятым на upload — рантайм-смена требует re-upload (иначе toggle не долетит до GPU)
    }
    bool isCoulombEnabled() const { return world().isCoulombEnabled(); }
    void setGravity(const Vec3f& gravity) {
        world().setGravity(gravity);
        notifySceneEdited(); // gravity в VRAM устарела — нужен re-upload (wall-kernel читает её как СИЛУ)
    }
    Vec3f getGravity() const { return world().getGravity(); }
    void setNeighborListCutoff(float cutoff) {
        world().getNeighborList().setCutoff(cutoff);
        notifySceneEdited(); // меняет listRadius/cutoffSqr — GPU должен перезалить uniforms
    }
    float getNeighborListCutoff() const { return world().getNeighborList().cutoff(); }
    void setNeighborListSkin(float skin) {
        world().getNeighborList().setSkin(skin);
        notifySceneEdited();
    }
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
        // Под активным GPU подтянуть свежие позиции ДО вставки: addAtom вставляет
        // mobile-атом в середину массива (swap для инварианта "mobile первыми"),
        // поэтому слепое слияние префикса на upload перезатёрло бы новый атом.
        // Синк один раз на батч (первый append снимает dirty), дальше — no-op.
        syncGpuBeforeEdit();
        world().getAtomStorage().addAtom(startCoords, startSpeed, type, fixed);
        invalidateMetricsCache();
    }
    void finalizeAtomBatch() {
        world().getGrid().rebuild(world().getAtomStorage().xDataSpan(), world().getAtomStorage().yDataSpan(),
                                  world().getAtomStorage().zDataSpan());
        world().getNeighborList().clear();
        notifySceneEdited(); // батч мог изменить контент при том же числе атомов (load той же длины)
    }
    void clear();

private:
    friend class SimulationStateIO;

    struct WorldState {
        // Конструктор и деструктор out-of-line: unique_ptr<GpuResidentPhysics>
        // требует полного типа в точках конструирования/уничтожения, а в
        // заголовке он только forward-declared.
        explicit WorldState(Vec3f size, Vec3f renderOffset);
        ~WorldState();
        WorldState(WorldState&&) = delete;
        WorldState& operator=(WorldState&&) = delete;

        World world;
        Integrator integrator;
        ForceField forceField_;
        float Dt = 0.01f;
        size_t sim_step = 0;
        float sim_time_ns = 0.0f;
        bool bondFormationEnabled_ = false;
        mutable bool metricsCacheValid_ = false;
        mutable EnergyMetrics::Snapshot metricsCache_{};

        // GPU-режим (opt-in). gpu == nullptr → CPU-путь.
        std::unique_ptr<GpuResidentPhysics> gpu;
        bool cpuPositionsDirty = false; // VRAM новее AtomStorage, нужен download
        int stepsSinceDispCheck = 0;    // шагов с момента ПОСЛЕДНЕГО запуска (kick) async disp-check; каденция запуска
        // Версия CPU-сцены против версии, залитой в VRAM. Расхождение → re-upload.
        // Ловит правки контента при неизменном числе атомов (скорости тулом, load
        // той же длины, resize, cutoff), которые проверка по size пропускает.
        uint64_t cpuSceneVersion = 0;
        uint64_t gpuUploadedSceneVersion = 0;
    };

    StepData makeStepData();
    StepData makeStepData(WorldState& state);
    void updateState(WorldState& state);
    void updateStateGpu(WorldState& state);
    // Заливает активную CPU-сцену в резидентный GPU (NL Full + полный upload).
    // Общий путь для входа в GPU-режим и для пере-синка после правки сцены.
    void uploadSceneToGpu(WorldState& state);
    WorldState& activeState();
    const WorldState& activeState() const;
    void invalidateMetricsCache() const { activeState().metricsCacheValid_ = false; }
    void refreshMetricsCache() const;

    std::vector<std::unique_ptr<WorldState>> worlds_;
    WorldId activeWorldIndex_ = 0;
};
