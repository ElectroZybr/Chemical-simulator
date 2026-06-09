#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

// GPU counting-sort cell-list (Шаг 2a, shadow-only). Строит тот же CSR
// `atomsInCells`, что и SpatialGrid::rebuild, но целиком на GPU из буфера
// позиций. НЕ подключён к physics-шагу: это теневая инфраструктура под
// будущий GPU NeighborList build, верифицируемая bench-гейтом parity.
//
// Владеет своими буферами (cellCounts/cellOffsets/cellCursors/atomCells/
// atomsInCells + scan scratch) и оркестрирует диспатчи. Отдельно от
// GpuResidentPhysics: 2a не трогает updateStateGpu/step.
//
// Контракт маппинга клетки задаётся ИЗВНЕ: вызывающий передаёт ту же размерность
// сетки (size, cellSize, cellCount), что у его SpatialGrid — тогда GPU биннит
// идентично CPU (одна формула в шейдере, см. gpu_cell_list.wgsl).
class GpuNeighborListBuilder {
public:
    GpuNeighborListBuilder();
    ~GpuNeighborListBuilder();

    GpuNeighborListBuilder(const GpuNeighborListBuilder&) = delete;
    GpuNeighborListBuilder& operator=(const GpuNeighborListBuilder&) = delete;

    // Строит cell-list на GPU из CPU-позиций (AoS x/y/z спанами длины atomCount).
    // sizeX/Y/Z, cellSize, cellCount — параметры сетки, совпадающие с CPU
    // SpatialGrid вызывающего. Блокирующе (для bench/debug): дренит очередь.
    void build(const std::vector<float>& posX, const std::vector<float>& posY, const std::vector<float>& posZ,
               uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ, float cellSize, uint32_t cellCount);

    // Шаг 2b: строит GPU Full NeighborList (CSR) в SHADOW-буфера поверх cell-list.
    // Сначала прогоняет build() (cell-list), затем count_neighbors_full -> scan ->
    // write_neighbors_full. listRadiusSqr — r_list^2 (cutoff+skin)^2, совпадает с
    // CPU NeighborList::listRadiusSqr_. Блокирующе. НЕ трогает GpuResidentPhysics:
    // это shadow-инфраструктура под будущий 2c, верифицируется BM_GpuNeighborList.
    void buildNeighborListFull(const std::vector<float>& posX, const std::vector<float>& posY, const std::vector<float>& posZ,
                               uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ, float cellSize, uint32_t cellCount,
                               float listRadiusSqr);

    // Шаг 2c: тот же Full NL build, но позиции уже в VRAM (резидентный буфер
    // GpuResidentPhysics::positions_). Вместо CPU-аплоада делаем GPU->GPU copy
    // srcPositions -> positions_ (без скачивания позиций в CPU — это и есть смысл
    // резидентного режима). srcPositions: vec4<f32> AoS длиной >= atomCount,
    // usage обязан включать CopySrc. После вызова результат в SHADOW-буферах
    // builder'а (nlOffsets()/nlNeighbors()/totalNeighbors()); владелец копирует
    // их GPU->GPU в свои резидентные nlOffsets_/nlNeighbors_. Блокирующе.
    void buildNeighborListFullFromGpuPositions(wgpu::Buffer srcPositions, uint32_t atomCount, uint32_t sizeX, uint32_t sizeY,
                                               uint32_t sizeZ, float cellSize, uint32_t cellCount, float listRadiusSqr);

    // Readback результатов последнего build (для parity-гейта). Блокирующие.
    // cellOffsets: exclusive scan длиной cellCount (атомов в клетке = off[c+1]-off[c]
    // только если есть off[cellCount]; здесь читаем ровно cellCount значений и
    // последний total отдаём отдельно через totalScanned()).
    [[nodiscard]] std::vector<uint32_t> readbackCellOffsets() const;
    [[nodiscard]] std::vector<uint32_t> readbackCellCounts() const;
    [[nodiscard]] std::vector<uint32_t> readbackAtomsInCells() const;

    // Readback NL (Шаг 2b). nlOffsets длиной atomCount+1 (как CPU offsets_):
    // nlOffsets[i+1]-nlOffsets[i] = соседей у i, nlOffsets[atomCount] = всего.
    // nlNeighbors читается ровно total элементов (nlOffsets[atomCount]).
    [[nodiscard]] std::vector<uint32_t> readbackNlOffsets() const;
    [[nodiscard]] std::vector<uint32_t> readbackNlNeighbors(uint32_t total) const;
    [[nodiscard]] std::vector<uint32_t> readbackNeighborCounts() const;

    [[nodiscard]] uint32_t atomCount() const noexcept { return atomCount_; }
    [[nodiscard]] uint32_t cellCount() const noexcept { return cellCount_; }

    // Шаг 2c: GPU-доступ к результату последнего NL build (без readback). Владелец
    // (GpuResidentPhysics) копирует эти буфера GPU->GPU в свои резидентные.
    //   nlOffsetsBuffer():   u32, длина atomCount+1 (CSR offsets, [atomCount]=total).
    //   nlNeighborsBuffer(): u32, длина >= totalNeighbors() (плоские индексы соседей).
    //   totalNeighbors():    nlOffsets[atomCount], известно после count+scan (скаляр-
    //                        ный readback внутри build). Точный размер для copy/grow.
    // Все три валидны только сразу после build*-вызова. Буфера имеют CopySrc.
    [[nodiscard]] wgpu::Buffer nlOffsetsBuffer() const noexcept { return *nlOffsets_; }
    [[nodiscard]] wgpu::Buffer nlNeighborsBuffer() const noexcept { return *nlNeighbors_; }
    [[nodiscard]] uint32_t totalNeighbors() const noexcept { return lastTotalNeighbors_; }

private:
    void ensureInitialized();
    void ensureCapacity(uint32_t atomCount, uint32_t cellCount);

    // Кодирует cell-list пайплайн (clear->count->scan->copy->scatter) в открытый
    // pass. Общий код build() и buildNeighborListFull(). Состояние (atomCount_/
    // cellCount_/...) и слоты должны быть уже подготовлены вызывающим.
    void encodeCellList(wgpu::ComputePassEncoder& pass);

    // Заливает позиции (AoS x/y/z -> vec4) в positions_. n == atomCount_.
    void uploadPositions(const std::vector<float>& posX, const std::vector<float>& posY, const std::vector<float>& posZ);

    // Общее тело Full NL build ПОСЛЕ того, как positions_ уже заполнен (CPU-аплоадом
    // в buildNeighborListFull или GPU->GPU copy в …FromGpuPositions) и состояние
    // (atomCount_/cellCount_/sizeXYZ/cellSize/listRadiusSqr_) выставлено. Делает
    // ensureCapacity/prepareSlots, прогоняет cell-list+count+scan, скалярный
    // readback total в lastTotalNeighbors_, grow nlNeighbors_, write_neighbors_full.
    // НЕ трогает positions_ — позиции должен залить вызывающий. Блокирующе.
    void buildNeighborListFullCore();

    // Рекурсивный иерархический exclusive scan: src[0..n) -> dst[0..n).
    // level индексирует scratch-буфера blockSums. Кодирует диспатчи в pass.
    // Возвращает ничего — записывает dst. Все буфера-аргументы уже capacity-OK.
    void encodeScanExclusive(wgpu::ComputePassEncoder& pass, wgpu::Buffer src, uint32_t n, wgpu::Buffer dst, int level);

    // n-слоты единого uniform-буфера. Несколько диспатчей в ОДНОМ submit не могут
    // делить один uniform с разными n: все queue.writeBuffer применяются ДО
    // выполнения command buffer, и последняя запись затирает прочие. Поэтому
    // каждое уникальное n живёт в своём слоте (dynamic offset). prepareSlots
    // собирает все нужные n за build, slotOffset отдаёт байтовый offset для n.
    void prepareSlots();
    void addSlot(uint32_t n);
    [[nodiscard]] uint32_t slotOffset(uint32_t n) const;

    // Блокирующий readback u32-буфера: count элементов со смещения srcOffsetElems.
    [[nodiscard]] std::vector<uint32_t> readU32(const wgpu::raii::Buffer& src, uint32_t count, uint32_t srcOffsetElems = 0) const;

    bool initialized_ = false;

    uint32_t atomCount_ = 0;
    uint32_t cellCount_ = 0;
    uint32_t lastTotalNeighbors_ = 0; // nlOffsets[atomCount] последнего NL build (2c GPU-доступ)

    // Размерность сетки текущего build (для шейдера и валидации).
    uint32_t sizeX_ = 0, sizeY_ = 0, sizeZ_ = 0;
    float cellSize_ = 0.0f;
    float listRadiusSqr_ = 0.0f; // r_list^2 для NL-фильтра (Шаг 2b; пишется в Params)

    // Раздельные layout'ы на ядро. Единый layout невозможен: WGSL-модуль
    // содержит 9 storage-буферов, а max_storage_buffers_per_shader_stage == 8.
    // Каждый layout содержит ТОЛЬКО биндинги своего ядра (<=3 storage), номера
    // биндингов совпадают с объявлением в шейдере. Access (ro/rw) совпадает с
    // шейдерным: ro -> ReadOnlyStorage, read_write -> Storage.
    wgpu::raii::BindGroupLayout clearLayout_;   // {0,2}
    wgpu::raii::BindGroupLayout countLayout_;   // {0,1,2,8}
    wgpu::raii::BindGroupLayout scanLayout_;    // {0,3,4,5} (scan_block + add_block_offsets)
    wgpu::raii::BindGroupLayout copyLayout_;    // {0,6,7}
    wgpu::raii::BindGroupLayout scatterLayout_; // {0,7,8,9}
    // Шаг 2b NL-проходы (читают cell-list 6/8/9 как plain). 5 и 6 storage <= 8.
    wgpu::raii::BindGroupLayout nlCountLayout_; // {0,1,6,8,9,10}
    wgpu::raii::BindGroupLayout nlWriteLayout_; // {0,1,6,8,9,11,12}

    wgpu::raii::ComputePipeline clearPipeline_;
    wgpu::raii::ComputePipeline countPipeline_;
    wgpu::raii::ComputePipeline scanBlockPipeline_;
    wgpu::raii::ComputePipeline addOffsetsPipeline_;
    wgpu::raii::ComputePipeline copyCursorsPipeline_;
    wgpu::raii::ComputePipeline scatterPipeline_;
    wgpu::raii::ComputePipeline nlCountPipeline_; // count_neighbors_full
    wgpu::raii::ComputePipeline nlWritePipeline_; // write_neighbors_full

    // Буфера cell-list.
    wgpu::raii::Buffer positions_;     // vec4<f32>, atomCapacity
    wgpu::raii::Buffer cellCounts_;    // u32, cellCapacity (atomic в count)
    wgpu::raii::Buffer cellOffsets_;   // u32, cellCapacity (exclusive scan)
    wgpu::raii::Buffer cellCursors_;   // u32, cellCapacity (atomic в scatter)
    wgpu::raii::Buffer atomCells_;     // u32, atomCapacity
    wgpu::raii::Buffer atomsInCells_;  // u32, atomCapacity

    // Шаг 2b NL shadow-буфера. neighborCounts/nlOffsets вмещают atomCount+1
    // (хвостовой 0-слот, чтобы exclusive scan длиной atomCount+1 дал nlOffsets
    // [atomCount]=total, как CPU offsets_). nlNeighbors с щедрым запасом
    // (overflow-обработка — задача 2c; здесь только тень).
    wgpu::raii::Buffer neighborCounts_; // u32, atomCapacity+1
    wgpu::raii::Buffer nlOffsets_;       // u32, atomCapacity+1 (exclusive scan)
    wgpu::raii::Buffer nlNeighbors_;     // u32, nlNeighborsCapacity_
    uint32_t nlNeighborsCapacity_ = 0;

    wgpu::raii::Buffer paramsUniform_; // Params-слоты по kUniformSlotStride (см. .cpp)
    uint32_t paramsCapacitySlots_ = 0; // вместимость paramsUniform_ в слотах

    // n -> байтовый dynamic-offset в paramsUniform_. Заполняется prepareSlots()
    // на каждый build (n-значения зависят от cellCount/atomCount/глубины scan).
    std::unordered_map<uint32_t, uint32_t> slotOf_;
    std::vector<uint32_t> slotValues_; // n по индексу слота (для записи)

    // Scan scratch: по одной паре буферов на уровень рекурсии (blockSums и его
    // exclusive-скан). Растут лениво под глубину. Длина уровня L =
    // ceil(len_{L-1} / SCAN_BLOCK).
    struct ScanLevel {
        wgpu::raii::Buffer sums;        // суммы блоков уровня (вход следующего)
        wgpu::raii::Buffer sumsScanned; // их exclusive scan
        uint32_t capacity = 0;
    };
    std::vector<ScanLevel> scanLevels_;

    // Readback-буфера (MapRead) — растут под максимум(atomCapacity, cellCapacity).
    wgpu::raii::Buffer readback_;
    uint32_t readbackCapacity_ = 0;

    uint32_t atomCapacity_ = 0;
    uint32_t cellCapacity_ = 0;
    // Scan-scratch покрывает максимум всех сканируемых длин: cellCount (cell-list)
    // и atomCount+1 (NL offsets). Декаплено от cellCapacity_, т.к. atomCount+1
    // может быть больше cellCount (плотные сцены).
    uint32_t scanCapacity_ = 0;

    // Per-kernel bind group билдеры. Каждый биндит ровно слоты своего layout'а.
    // Биндинг 0 (uniform) — с dynamic offset, поэтому size = sizeof(Params) и
    // конкретный n выбирается offset'ом в setBindGroup.
    wgpu::BindGroup makeClearBG();   // {0,2}
    wgpu::BindGroup makeCountBG();   // {0,1,2,8}
    wgpu::BindGroup makeScanBG(wgpu::Buffer scanIn, uint64_t scanInBytes, wgpu::Buffer scanOut, uint64_t scanOutBytes,
                               wgpu::Buffer blockSums, uint64_t blockSumsBytes); // {0,3,4,5}
    wgpu::BindGroup makeCopyBG();    // {0,6,7}
    wgpu::BindGroup makeScatterBG(); // {0,7,8,9}
    wgpu::BindGroup makeNlCountBG(); // {0,1,6,8,9,10}
    wgpu::BindGroup makeNlWriteBG(); // {0,1,6,8,9,11,12}

    // Размеры биндингов в байтах под текущие capacity.
    uint64_t atomVec4Bytes_() const { return static_cast<uint64_t>(atomCapacity_) * 16; }
    uint64_t atomU32Bytes_() const { return static_cast<uint64_t>(atomCapacity_) * 4; }
    uint64_t cellU32Bytes_() const { return static_cast<uint64_t>(cellCapacity_) * 4; }
};
