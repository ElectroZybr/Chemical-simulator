#pragma once

#include <cstddef>
#include <cstdint>

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
    // (для решения нужен ли NL rebuild). Читает 4 байта, не все позиции.
    float maxDisplacementSqr();

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    // Число атомов, под которое сейчас залиты резидентные буфера. Сравнивается
    // с CPU AtomStorage::size() для детекта правки сцены при включённом GPU.
    [[nodiscard]] uint32_t totalCount() const noexcept { return totalCount_; }

private:
    void ensureInitialized();
    void ensureCapacity(size_t totalCount, size_t mobileCount, size_t neighborCount);
    void rebuildBindGroups();

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
    size_t nlOffsetsCapacity_ = 0;
    size_t nlNeighborsCapacity_ = 0;
};
