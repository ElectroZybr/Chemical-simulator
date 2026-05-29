#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

class AtomStorage;
class NeighborList;
class LJForceField;

// Резидентная GPU-физика: позиции/скорости/силы живут в VRAM между шагами,
// CPU их не качает в hot loop. Это и есть «физика на GPU» (в отличие от
// GpuPairForceCompute, который оффлоадит одну операцию с readback каждый раз).
//
// Ограничения GPU-режима (LJ-only): wall/bond/Coulomb выключены, NeighborList
// в режиме Full (каждая пара дважды, force loop пишет только в свой forceX —
// нет race). Эти ограничения — следствие резидентности: если бы силы читал
// CPU (bond/wall), пришлось бы качать позиции каждый шаг.
//
// Шаг повторяет CPU velocity Verlet (VerletScheme + StepOps::confineToBox):
//   predict -> confine -> swap(pf<->f, parity) -> zero(f, total) -> LJ -> correct
// swap реализован ping-pong'ом двух force-буферов через parity-бит и две
// пред-собранные bind-группы (не копирование).
class GpuResidentPhysics {
public:
    GpuResidentPhysics();
    ~GpuResidentPhysics();

    GpuResidentPhysics(const GpuResidentPhysics&) = delete;
    GpuResidentPhysics& operator=(const GpuResidentPhysics&) = delete;

    // Заливает полное состояние атомов + NL из CPU в VRAM. Вызывается при входе
    // в GPU-режим и после любой структурной правки (add/remove atom, NL rebuild).
    void uploadFromCpu(const AtomStorage& atoms, const NeighborList& neighborList, const LJForceField& ljForceField,
                       float worldSizeX, float worldSizeY, float worldSizeZ);

    // Только NL (offsets+neighbors) + refPos для displacement-проверки. Вызывается
    // после CPU NeighborList::build, без перезаливки позиций/скоростей.
    void uploadNeighborList(const NeighborList& neighborList);

    // Один резидентный шаг (dt, accelDamping как у CPU Integrator). Ничего не
    // качает CPU<->GPU. Допускает батчинг (несколько step() подряд до sync).
    void step(float dt, float accelDamping);

    // Скачивает позиции (и опц. скорости) обратно в CPU AtomStorage. Нужно для
    // NL rebuild, рендера, метрик — редкие sync-точки, не каждый шаг.
    void downloadToCpu(AtomStorage& atoms, bool withVelocities = true);

    // Возвращает максимум |pos - refPos|^2 по mobile-атомам через GPU-редукцию
    // (для решения нужен ли NL rebuild). Читает 4 байта, но БЛОКИРУЕТ (dev.poll)
    // — дренит GPU-очередь. Оставлен для совместимости/backstop.
    float maxDisplacementSqr();

    // Асинхронный disp-check без блокирующего poll'а (резидентный пайплайн не
    // сериализуется). begin запускает редукцию + неблокирующий mapAsync;
    // tryConsume неблокирующе прокручивает callback и отдаёт результат когда
    // готов; finishBlocking форсирует завершение (hard backstop по возрасту);
    // discard безопасно сносит pending (после reupload refPos устарел).
    // Single-in-flight: begin — no-op пока предыдущий не завершён.
    void beginMaxDisplacementSqrAsync();
    [[nodiscard]] std::optional<float> tryConsumeMaxDisplacementSqr();
    // Дождаться pending-результата блокирующе. PRECONDITION: вызывать только когда
    // dispCheckPending() — иначе dispMap_.done может быть stale-true от прошлого
    // и read пойдёт по unmapped-буферу. Все вызовы охраняются dispCheckPending().
    float finishMaxDisplacementSqrBlocking();
    void discardPendingDisplacementCheck();
    [[nodiscard]] bool dispCheckPending() const noexcept { return dispCheckPending_; }
    [[nodiscard]] int dispCheckAgeSteps() const noexcept { return dispCheckAgeSteps_; }
    // Телеметрия (бенч/диагностика): сколько disp-check'ов забрано async без столла
    // против сколько ушло в блокирующий backstop. Доля backstop≈1 => бенч глушит async.
    [[nodiscard]] uint64_t dispConsumeCount() const noexcept { return dispConsumeCount_; }
    [[nodiscard]] uint64_t dispBackstopCount() const noexcept { return dispBackstopCount_; }
    [[nodiscard]] uint64_t dispBeginCount() const noexcept { return dispBeginCount_; }

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    // Число атомов, под которое сейчас залиты резидентные буфера. Сравнивается
    // с CPU AtomStorage::size() для детекта правки сцены при включённом GPU.
    [[nodiscard]] uint32_t totalCount() const noexcept { return totalCount_; }

private:
    void ensureInitialized();
    void ensureCapacity(size_t totalCount, size_t mobileCount, size_t neighborCount);
    void rebuildBindGroups();
    // Запуск GPU-редукции смещения + mapAsync (без блокировки). Общий для async-
    // begin и блокирующего maxDisplacementSqr.
    void submitDisplacementReductionAndMap();
    // Чтение готового результата dispReadback_ (mapped), unmap, снятие pending.
    // Вызывать только когда dispMap_.done. Возвращает max|disp|^2.
    float readDisplacementResultAndClear();

    bool initialized_ = false;
    bool ljTableUploaded_ = false;

    uint32_t mobileCount_ = 0;
    uint32_t totalCount_ = 0;
    int parity_ = 0; // какой из forces_[2] сейчас «current»

    float cutoffSqr_ = 0.0f;
    float worldMax_[3] = {0, 0, 0};

    // Pipelines
    wgpu::raii::ComputePipeline ljPipeline_;
    wgpu::raii::ComputePipeline predictPipeline_;
    wgpu::raii::ComputePipeline confinePipeline_;
    wgpu::raii::ComputePipeline zeroPipeline_;
    wgpu::raii::ComputePipeline correctPipeline_;
    wgpu::raii::ComputePipeline displacementPipeline_;

    wgpu::raii::BindGroupLayout ljLayout_;
    wgpu::raii::BindGroupLayout intLayout_;
    wgpu::raii::BindGroupLayout dispLayout_;

    // Резидентные буфера
    wgpu::raii::Buffer positions_;
    wgpu::raii::Buffer velocities_;
    wgpu::raii::Buffer forces_[2]; // ping-pong: current / prev
    wgpu::raii::Buffer invMass_;
    wgpu::raii::Buffer types_;
    wgpu::raii::Buffer nlOffsets_;
    wgpu::raii::Buffer nlNeighbors_;
    wgpu::raii::Buffer ljPairs_;
    wgpu::raii::Buffer refPos_;          // позиции на момент последнего NL build
    wgpu::raii::Buffer dispFlag_;        // 1×u32, atomicMax bitcast displacement^2
    wgpu::raii::Buffer dispReadback_;    // MapRead для dispFlag_
    wgpu::raii::Buffer posReadback_;     // MapRead для downloadToCpu
    wgpu::raii::Buffer velReadback_;

    wgpu::raii::Buffer ljUniform_;
    wgpu::raii::Buffer intUniform_;
    wgpu::raii::Buffer dispUniform_;

    // Bind-группы для parity 0 и 1 (current/prev меняются местами).
    wgpu::raii::BindGroup ljBindGroup_[2];
    wgpu::raii::BindGroup intBindGroup_[2];
    wgpu::raii::BindGroup zeroBindGroup_[2];
    wgpu::raii::BindGroup dispBindGroup_;

    size_t atomCapacity_ = 0;

    // Состояние async disp-check (single-in-flight). Callback (mapAsync,
    // AllowSpontaneous) по контракту webgpu.h может сработать на ПРОИЗВОЛЬНОМ
    // потоке — поэтому флаги атомарные (callback store, main-thread load); сам
    // callback больше ничего не делает (без re-entrant webgpu-вызовов). dispMap_
    // — член (стабильный адрес, переживает begin); read/unmap на main-thread.
    struct DispMapState {
        std::atomic<bool> done{false};
        std::atomic<bool> ok{false};
    };
    DispMapState dispMap_{};
    bool dispCheckPending_ = false; // readback запущен, ещё не consume/finish
    int dispCheckAgeSteps_ = 0;     // physics-шагов с момента begin (для backstop)

    // Телеметрия async disp-check (для бенч-матрицы: доля backstop vs async-consume).
    uint64_t dispBeginCount_ = 0;
    uint64_t dispConsumeCount_ = 0;   // async-результат забран без столла
    uint64_t dispBackstopCount_ = 0;  // пришлось блокирующе дождаться (finishBlocking)
    uint64_t dispDiscardCount_ = 0;
    size_t nlOffsetsCapacity_ = 0;
    size_t nlNeighborsCapacity_ = 0;
};
