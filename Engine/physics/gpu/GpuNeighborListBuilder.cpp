#include "GpuNeighborListBuilder.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "Rendering/WGPUContext.h"

#include "generated/shaders/gpu_cell_list.wgsl.h"

namespace {

// Должно совпадать с Params в gpu_cell_list.wgsl (std140-совместимая раскладка:
// 8×u32/f32 = 32 байта, кратно 16).
struct Params {
    uint32_t sizeX;
    uint32_t sizeY;
    uint32_t sizeZ;
    uint32_t atomCount;
    float cellSize;
    uint32_t n;
    float listRadiusSqr; // r_list^2 (Шаг 2b); 0 на cell-list-only build
    uint32_t cellCount;  // всего клеток (Шаг 2b NL-обходу для end-границы CSR)
};
static_assert(sizeof(Params) == 32, "Params должен совпадать с std140-раскладкой gpu_cell_list.wgsl (8×4=32)");

constexpr uint32_t kScanThreads = 256u;
constexpr uint32_t kScanBlock = 512u; // 2 элемента на поток (см. шейдер)

// Шаг между слотами uniform-буфера: WebGPU требует выравнивания dynamic-offset
// по minUniformBufferOffsetAlignment (зависит от девайса, максимум по дефолту
// 256). 256 безопасно для любого девайса.
constexpr uint32_t kUniformSlotStride = 256u;

// Запас 1.5x как у GpuResidentPhysics (storage-буфера растут с запасом).
constexpr uint32_t headroom(uint32_t n) { return n + n / 2u + 1u; }

constexpr uint32_t ceilDiv(uint32_t a, uint32_t b) { return (a + b - 1u) / b; }

wgpu::ShaderModule makeModule(std::string_view wgsl) {
    WGPUShaderSourceWGSL d{};
    d.chain.sType = WGPUSType_ShaderSourceWGSL;
    d.code = wgpu::StringView(wgsl);
    wgpu::ShaderModuleDescriptor sm{};
    sm.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&d);
    return WGPUContext::instance().device()->createShaderModule(sm);
}

wgpu::raii::ComputePipeline makePipeline(wgpu::BindGroupLayout bgl, wgpu::ShaderModule shader, const char* entry) {
    wgpu::Device dev = *WGPUContext::instance().device();
    wgpu::PipelineLayoutDescriptor pl{};
    pl.bindGroupLayoutCount = 1;
    pl.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&bgl);
    wgpu::PipelineLayout layout = dev.createPipelineLayout(pl);
    wgpu::ComputePipelineDescriptor pd{};
    pd.layout = layout;
    pd.compute.module = shader;
    pd.compute.entryPoint = wgpu::StringView(entry);
    return dev.createComputePipeline(pd);
}

} // namespace

GpuNeighborListBuilder::GpuNeighborListBuilder() = default;
GpuNeighborListBuilder::~GpuNeighborListBuilder() = default;

void GpuNeighborListBuilder::ensureInitialized() {
    if (initialized_) {
        return;
    }
    wgpu::Device dev = *WGPUContext::instance().device();
    if (dev == nullptr) {
        throw std::runtime_error("GpuNeighborListBuilder: WGPUContext device not initialized");
    }

    // Раздельные layout'ы на ядро (max_storage_buffers_per_shader_stage == 8 не
    // даёт сделать единый layout с 9 storage). Хелпер строит layout из списка
    // (binding, type); биндинг 0 всегда uniform с dynamic offset. Типы (ro/rw)
    // должны совпадать с объявлением в gpu_cell_list.wgsl.
    using BBT = wgpu::BufferBindingType;
    auto makeLayout = [](std::initializer_list<std::pair<uint32_t, BBT>> binds, const char* label) {
        std::vector<wgpu::BindGroupLayoutEntry> e(binds.size());
        size_t i = 0;
        for (auto [binding, type] : binds) {
            e[i].binding = binding;
            e[i].visibility = wgpu::ShaderStage::Compute;
            e[i].buffer.type = type;
            if (binding == 0u) {
                e[i].buffer.hasDynamicOffset = true; // uniform Params: n-слот по offset
            }
            ++i;
        }
        return WGPUContext::instance().createBindGroupLayout(e, label);
    };

    clearLayout_ = makeLayout({{0, BBT::Uniform}, {2, BBT::Storage}}, "GNL_ClearBGL");
    countLayout_ = makeLayout({{0, BBT::Uniform}, {1, BBT::ReadOnlyStorage}, {2, BBT::Storage}, {8, BBT::Storage}}, "GNL_CountBGL");
    scanLayout_ = makeLayout({{0, BBT::Uniform}, {3, BBT::ReadOnlyStorage}, {4, BBT::Storage}, {5, BBT::Storage}}, "GNL_ScanBGL");
    copyLayout_ = makeLayout({{0, BBT::Uniform}, {6, BBT::ReadOnlyStorage}, {7, BBT::Storage}}, "GNL_CopyBGL");
    scatterLayout_ =
        makeLayout({{0, BBT::Uniform}, {7, BBT::Storage}, {8, BBT::Storage}, {9, BBT::Storage}}, "GNL_ScatterBGL");
    // Шаг 2b NL-проходы. cellOffsets(6)/atomCells(8)/atomsInCells(9) NL читает, но
    // их access в шейдере фиксирован: 6=read (ReadOnlyStorage), 8/9=read_write
    // (Storage) — layout обязан совпасть с объявлением модуля, даже если NL их не
    // пишет. nlOffsets(11)=read (ReadOnlyStorage), остальные NL-буфера read_write.
    nlCountLayout_ = makeLayout({{0, BBT::Uniform},
                                 {1, BBT::ReadOnlyStorage},
                                 {6, BBT::ReadOnlyStorage},
                                 {8, BBT::Storage},
                                 {9, BBT::Storage},
                                 {10, BBT::Storage}},
                                "GNL_NlCountBGL");
    nlWriteLayout_ = makeLayout({{0, BBT::Uniform},
                                 {1, BBT::ReadOnlyStorage},
                                 {6, BBT::ReadOnlyStorage},
                                 {8, BBT::Storage},
                                 {9, BBT::Storage},
                                 {11, BBT::ReadOnlyStorage},
                                 {12, BBT::Storage}},
                                "GNL_NlWriteBGL");

    wgpu::ShaderModule mod = makeModule(gpu_cell_listWGSL);
    clearPipeline_ = makePipeline(*clearLayout_, mod, "clear_counts");
    countPipeline_ = makePipeline(*countLayout_, mod, "count_cells");
    scanBlockPipeline_ = makePipeline(*scanLayout_, mod, "scan_block");
    addOffsetsPipeline_ = makePipeline(*scanLayout_, mod, "add_block_offsets");
    copyCursorsPipeline_ = makePipeline(*copyLayout_, mod, "copy_offsets_to_cursors");
    scatterPipeline_ = makePipeline(*scatterLayout_, mod, "scatter_atoms");
    nlCountPipeline_ = makePipeline(*nlCountLayout_, mod, "count_neighbors_full");
    nlWritePipeline_ = makePipeline(*nlWriteLayout_, mod, "write_neighbors_full");

    // paramsUniform_ растёт под число n-слотов в prepareSlots (зависит от build).
    initialized_ = true;
}

void GpuNeighborListBuilder::ensureCapacity(uint32_t atomCount, uint32_t cellCount) {
    // Все буфера биндятся безусловно (общий layout), значит должны существовать
    // даже при 0 — резервируем минимум 1 (как GpuResidentPhysics::ensureCapacity).
    const uint32_t atomNeed = std::max<uint32_t>(atomCount, 1u);
    const uint32_t cellNeed = std::max<uint32_t>(cellCount, 1u);

    const wgpu::BufferUsage st = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;

    if (atomNeed > atomCapacity_) {
        const uint32_t cap = headroom(atomNeed);
        positions_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 16, st, "GNL_Pos");
        atomCells_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 4, st, "GNL_AtomCells");
        atomsInCells_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 4, st, "GNL_AtomsInCells");
        // Шаг 2b: neighborCounts/nlOffsets под atomCount+1 (хвостовой 0-слот для
        // total в exclusive-scan длиной atomCount+1). cap уже headroom(atomNeed),
        // т.е. >= atomNeed+1 для atomNeed>=1 — слот [atomCount] всегда внутри.
        neighborCounts_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 4, st, "GNL_NeighborCounts");
        nlOffsets_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 4, st, "GNL_NlOffsets");
        atomCapacity_ = cap;
    }
    if (cellNeed > cellCapacity_) {
        const uint32_t cap = headroom(cellNeed);
        cellCounts_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 4, st, "GNL_CellCounts");
        cellOffsets_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 4, st, "GNL_CellOffsets");
        cellCursors_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 4, st, "GNL_CellCursors");
        cellCapacity_ = cap;
    }

    // Scan scratch покрывает максимум всех сканируемых длин: cellCapacity_
    // (cell-list scan) и atomCapacity_ (NL offsets scan на atomCount+1 <=
    // atomCapacity_). Декаплено от cellCapacity_: на плотных сценах atomCount+1
    // может превысить cellCount, и scan по нему ушёл бы глубже зарезервированного.
    const uint32_t scanNeed = std::max(cellCapacity_, atomCapacity_);
    if (scanNeed > scanCapacity_) {
        // Уровни до тех пор, пока длина уровня > 1. Уровень 0 сканирует scanNeed
        // элементов -> numBlocks0 сумм блоков; уровень L+1 сканирует суммы L.
        // Глубина = log_{SCAN_BLOCK}(scanNeed). Рантайм-рекурсия идёт по точному n.
        scanLevels_.clear();
        uint32_t levelLen = scanNeed;
        while (levelLen > 1u) {
            const uint32_t numBlocks = ceilDiv(levelLen, kScanBlock);
            ScanLevel lvl;
            const uint32_t bcap = std::max<uint32_t>(numBlocks, 1u);
            lvl.sums = WGPUContext::instance().createBuffer(static_cast<size_t>(bcap) * 4, st, "GNL_ScanSums");
            lvl.sumsScanned = WGPUContext::instance().createBuffer(static_cast<size_t>(bcap) * 4, st, "GNL_ScanSumsScanned");
            lvl.capacity = bcap;
            scanLevels_.push_back(std::move(lvl));
            levelLen = numBlocks;
        }
        // Гарантируем хотя бы один уровень (для scanNeed<=1 петля выше пуста, но
        // encodeScanExclusive на n<=1 не дёргает blockSums за пределами — всё же
        // даём 1 уровень, чтобы биндинг blockSums всегда был валиден).
        if (scanLevels_.empty()) {
            ScanLevel lvl;
            lvl.sums = WGPUContext::instance().createBuffer(4, st, "GNL_ScanSums");
            lvl.sumsScanned = WGPUContext::instance().createBuffer(4, st, "GNL_ScanSumsScanned");
            lvl.capacity = 1u;
            scanLevels_.push_back(std::move(lvl));
        }
        scanCapacity_ = scanNeed;
    }

    const uint32_t rbNeed = std::max(atomCapacity_, cellCapacity_);
    if (rbNeed > readbackCapacity_) {
        readback_ = WGPUContext::instance().createBuffer(static_cast<size_t>(rbNeed) * 4,
                                                         wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst, "GNL_Readback");
        readbackCapacity_ = rbNeed;
    }
}

// Биндинг 0 (uniform Params) одинаков везде: size = один слот, конкретный n
// выбирается dynamic-offset'ом в setBindGroup.
static wgpu::BindGroupEntry uniformEntry(wgpu::Buffer params) {
    wgpu::BindGroupEntry b{};
    b.binding = 0;
    b.buffer = params;
    b.size = sizeof(Params);
    return b;
}

wgpu::BindGroup GpuNeighborListBuilder::makeClearBG() {
    std::array<wgpu::BindGroupEntry, 2> b{};
    b[0] = uniformEntry(*paramsUniform_);
    b[1].binding = 2;
    b[1].buffer = *cellCounts_;
    b[1].size = cellU32Bytes_();
    return WGPUContext::instance().createBindGroup(*clearLayout_, b, "GNL_ClearBG");
}

wgpu::BindGroup GpuNeighborListBuilder::makeCountBG() {
    std::array<wgpu::BindGroupEntry, 4> b{};
    b[0] = uniformEntry(*paramsUniform_);
    b[1].binding = 1;
    b[1].buffer = *positions_;
    b[1].size = atomVec4Bytes_();
    b[2].binding = 2;
    b[2].buffer = *cellCounts_;
    b[2].size = cellU32Bytes_();
    b[3].binding = 8;
    b[3].buffer = *atomCells_;
    b[3].size = atomU32Bytes_();
    return WGPUContext::instance().createBindGroup(*countLayout_, b, "GNL_CountBG");
}

wgpu::BindGroup GpuNeighborListBuilder::makeScanBG(wgpu::Buffer scanIn, uint64_t scanInBytes, wgpu::Buffer scanOut,
                                                   uint64_t scanOutBytes, wgpu::Buffer blockSums, uint64_t blockSumsBytes) {
    std::array<wgpu::BindGroupEntry, 4> b{};
    b[0] = uniformEntry(*paramsUniform_);
    b[1].binding = 3;
    b[1].buffer = scanIn;
    b[1].size = scanInBytes;
    b[2].binding = 4;
    b[2].buffer = scanOut;
    b[2].size = scanOutBytes;
    b[3].binding = 5;
    b[3].buffer = blockSums;
    b[3].size = blockSumsBytes;
    return WGPUContext::instance().createBindGroup(*scanLayout_, b, "GNL_ScanBG");
}

wgpu::BindGroup GpuNeighborListBuilder::makeCopyBG() {
    std::array<wgpu::BindGroupEntry, 3> b{};
    b[0] = uniformEntry(*paramsUniform_);
    b[1].binding = 6;
    b[1].buffer = *cellOffsets_;
    b[1].size = cellU32Bytes_();
    b[2].binding = 7;
    b[2].buffer = *cellCursors_;
    b[2].size = cellU32Bytes_();
    return WGPUContext::instance().createBindGroup(*copyLayout_, b, "GNL_CopyBG");
}

wgpu::BindGroup GpuNeighborListBuilder::makeScatterBG() {
    std::array<wgpu::BindGroupEntry, 4> b{};
    b[0] = uniformEntry(*paramsUniform_);
    b[1].binding = 7;
    b[1].buffer = *cellCursors_;
    b[1].size = cellU32Bytes_();
    b[2].binding = 8;
    b[2].buffer = *atomCells_;
    b[2].size = atomU32Bytes_();
    b[3].binding = 9;
    b[3].buffer = *atomsInCells_;
    b[3].size = atomU32Bytes_();
    return WGPUContext::instance().createBindGroup(*scatterLayout_, b, "GNL_ScatterBG");
}

wgpu::BindGroup GpuNeighborListBuilder::makeNlCountBG() {
    // {0,1,6,8,9,10}: positions, cellOffsets, atomCells, atomsInCells (read-only
    // use), neighborCounts (write). Размеры по текущим capacity.
    std::array<wgpu::BindGroupEntry, 6> b{};
    b[0] = uniformEntry(*paramsUniform_);
    b[1].binding = 1;
    b[1].buffer = *positions_;
    b[1].size = atomVec4Bytes_();
    b[2].binding = 6;
    b[2].buffer = *cellOffsets_;
    b[2].size = cellU32Bytes_();
    b[3].binding = 8;
    b[3].buffer = *atomCells_;
    b[3].size = atomU32Bytes_();
    b[4].binding = 9;
    b[4].buffer = *atomsInCells_;
    b[4].size = atomU32Bytes_();
    b[5].binding = 10;
    b[5].buffer = *neighborCounts_;
    b[5].size = atomU32Bytes_();
    return WGPUContext::instance().createBindGroup(*nlCountLayout_, b, "GNL_NlCountBG");
}

wgpu::BindGroup GpuNeighborListBuilder::makeNlWriteBG() {
    // {0,1,6,8,9,11,12}: + nlOffsets (read), nlNeighbors (write).
    std::array<wgpu::BindGroupEntry, 7> b{};
    b[0] = uniformEntry(*paramsUniform_);
    b[1].binding = 1;
    b[1].buffer = *positions_;
    b[1].size = atomVec4Bytes_();
    b[2].binding = 6;
    b[2].buffer = *cellOffsets_;
    b[2].size = cellU32Bytes_();
    b[3].binding = 8;
    b[3].buffer = *atomCells_;
    b[3].size = atomU32Bytes_();
    b[4].binding = 9;
    b[4].buffer = *atomsInCells_;
    b[4].size = atomU32Bytes_();
    b[5].binding = 11;
    b[5].buffer = *nlOffsets_;
    b[5].size = atomU32Bytes_();
    b[6].binding = 12;
    b[6].buffer = *nlNeighbors_;
    b[6].size = static_cast<uint64_t>(nlNeighborsCapacity_) * 4;
    return WGPUContext::instance().createBindGroup(*nlWriteLayout_, b, "GNL_NlWriteBG");
}

void GpuNeighborListBuilder::addSlot(uint32_t n) {
    if (slotOf_.find(n) != slotOf_.end()) {
        return;
    }
    const uint32_t idx = static_cast<uint32_t>(slotValues_.size());
    slotOf_.emplace(n, idx * kUniformSlotStride);
    slotValues_.push_back(n);
}

uint32_t GpuNeighborListBuilder::slotOffset(uint32_t n) const {
    auto it = slotOf_.find(n);
    if (it == slotOf_.end()) {
        throw std::runtime_error("GpuNeighborListBuilder: n-slot not prepared (internal error)");
    }
    return it->second;
}

void GpuNeighborListBuilder::prepareSlots() {
    // Собираем все n, которые понадобятся за этот build: cellCount (clear/copy/
    // scan L0), atomCount (count/scatter), и numBlocks каждого уровня scan
    // (scan_block/add на уровнях >=1). Затем пишем все слоты ОДИН раз — после
    // submit'а каждый диспатч читает свой слот по dynamic-offset, не затирая
    // соседние (в отличие от перезаписи одного uniform несколькими writeBuffer).
    slotOf_.clear();
    slotValues_.clear();

    addSlot(cellCount_);
    addSlot(atomCount_);
    // Уровни scan cell-list: n начинается с cellCount, далее numBlocks каждого уровня.
    uint32_t levelLen = cellCount_;
    while (levelLen > 1u) {
        addSlot(levelLen);
        levelLen = ceilDiv(levelLen, kScanBlock);
    }
    addSlot(levelLen); // финальный (1 или 0) — для полноты, дёшево

    // Шаг 2b: уровни scan NL-offset'ов. Сканируем atomCount+1 элементов (хвостовой
    // 0 даёт total в nlOffsets[atomCount]). Слоты дёшевы и для cell-list-only build.
    uint32_t nlLen = atomCount_ + 1u;
    while (nlLen > 1u) {
        addSlot(nlLen);
        nlLen = ceilDiv(nlLen, kScanBlock);
    }
    addSlot(nlLen);

    // Растим uniform-буфер под число слотов (kUniformSlotStride на слот).
    const uint32_t needSlots = static_cast<uint32_t>(slotValues_.size());
    if (needSlots > paramsCapacitySlots_) {
        paramsUniform_ = WGPUContext::instance().createBuffer(static_cast<size_t>(needSlots) * kUniformSlotStride,
                                                              wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "GNL_Params");
        paramsCapacitySlots_ = needSlots;
    }

    // Пишем каждый слот (Params с его n) по своему offset. listRadiusSqr_ и
    // cellCount_ одинаковы во всех слотах (зависят от сцены, не от прохода).
    auto q = WGPUContext::instance().queue();
    for (uint32_t i = 0; i < needSlots; ++i) {
        Params p{sizeX_, sizeY_, sizeZ_, atomCount_, cellSize_, slotValues_[i], listRadiusSqr_, cellCount_};
        q->writeBuffer(*paramsUniform_, static_cast<uint64_t>(i) * kUniformSlotStride, &p, sizeof(p));
    }
}

void GpuNeighborListBuilder::encodeScanExclusive(wgpu::ComputePassEncoder& pass, wgpu::Buffer src, uint32_t n, wgpu::Buffer dst,
                                                 int level) {
    // Базовый случай: пусто/один элемент. Exclusive scan от <=1 элементов — это
    // [0]; запись делает scan_block с n<=1 сам (a0<n гейтит). Но при n==0 ничего.
    if (n == 0u) {
        return;
    }
    if (level >= static_cast<int>(scanLevels_.size())) {
        // Глубже, чем зарезервировано — не должно случаться (scanLevels_ покрывают
        // log_{SCAN_BLOCK}(cellCapacity)). Фейлим явно, не молча.
        throw std::runtime_error("GpuNeighborListBuilder: scan recursion exceeded reserved levels");
    }

    const uint32_t numBlocks = ceilDiv(n, kScanBlock);
    ScanLevel& lvl = scanLevels_[level];
    const uint64_t srcBytes = static_cast<uint64_t>(n) * 4; // привязываем ровно n (>=сколько шейдер трогает)
    const uint64_t lvlBytes = static_cast<uint64_t>(lvl.capacity) * 4;

    // n-слот этого уровня заранее записан prepareSlots(); диспатч выбирает его
    // dynamic-offset'ом, поэтому соседние диспатчи (с другими n) не мешают.
    uint32_t off = slotOffset(n);
    {
        wgpu::BindGroup bg = makeScanBG(src, srcBytes, dst, srcBytes, *lvl.sums, lvlBytes);
        pass.setBindGroup(0, bg, 1, &off);
        pass.setPipeline(*scanBlockPipeline_);
        pass.dispatchWorkgroups(numBlocks, 1, 1);
    }

    if (numBlocks > 1u) {
        // Рекурсивно сканируем суммы блоков (их numBlocks штук) в sumsScanned.
        encodeScanExclusive(pass, *lvl.sums, numBlocks, *lvl.sumsScanned, level + 1);
        // Прибавляем префикс блока к каждому элементу dst (n этого уровня).
        // add_block_offsets использует {0,3,4}; binding 5 (blockSums) layout всё
        // равно требует — даём *lvl.sums (ядром не трогается).
        uint32_t offAdd = slotOffset(n);
        wgpu::BindGroup bg = makeScanBG(*lvl.sumsScanned, lvlBytes, dst, srcBytes, *lvl.sums, lvlBytes);
        pass.setBindGroup(0, bg, 1, &offAdd);
        pass.setPipeline(*addOffsetsPipeline_);
        pass.dispatchWorkgroups(ceilDiv(n, kScanThreads), 1, 1);
    }
    // numBlocks==1: scan_block уже дал полный exclusive scan блока (его offset=0),
    // add не нужен.
}

void GpuNeighborListBuilder::build(const std::vector<float>& posX, const std::vector<float>& posY, const std::vector<float>& posZ,
                                   uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ, float cellSize, uint32_t cellCount) {
    ensureInitialized();

    const uint32_t n = static_cast<uint32_t>(posX.size());
    if (posY.size() != n || posZ.size() != n) {
        throw std::runtime_error("GpuNeighborListBuilder::build: pos arrays length mismatch");
    }
    if (static_cast<uint64_t>(sizeX) * sizeY * sizeZ != cellCount) {
        throw std::runtime_error("GpuNeighborListBuilder::build: cellCount != sizeX*sizeY*sizeZ");
    }

    atomCount_ = n;
    cellCount_ = cellCount;
    sizeX_ = sizeX;
    sizeY_ = sizeY;
    sizeZ_ = sizeZ;
    cellSize_ = cellSize;
    listRadiusSqr_ = 0.0f; // cell-list-only build: NL-фильтр не используется

    ensureCapacity(n, cellCount);
    prepareSlots(); // записать все n-слоты uniform до encode (см. prepareSlots)

    uploadPositions(posX, posY, posZ);

    wgpu::Device dev = *WGPUContext::instance().device();
    wgpu::CommandEncoder enc = dev.createCommandEncoder({});
    wgpu::ComputePassEncoder pass = enc.beginComputePass({});
    encodeCellList(pass);
    pass.end();
    wgpu::CommandBuffer cmd = enc.finish({});
    WGPUContext::instance().queue()->submit(1, &cmd);

    // Дренируем очередь — build блокирующий (bench/debug контракт).
    dev.poll(true, nullptr);
}

void GpuNeighborListBuilder::uploadPositions(const std::vector<float>& posX, const std::vector<float>& posY,
                                             const std::vector<float>& posZ) {
    const uint32_t n = atomCount_;
    if (n == 0u) {
        return;
    }
    // Позиции как vec4 AoS (x,y,z,0).
    std::vector<float> pos(static_cast<size_t>(n) * 4);
    for (uint32_t i = 0; i < n; ++i) {
        pos[i * 4 + 0] = posX[i];
        pos[i * 4 + 1] = posY[i];
        pos[i * 4 + 2] = posZ[i];
        pos[i * 4 + 3] = 0.0f;
    }
    WGPUContext::instance().queue()->writeBuffer(*positions_, 0, pos.data(), pos.size() * 4);
}

void GpuNeighborListBuilder::encodeCellList(wgpu::ComputePassEncoder& pass) {
    const uint32_t n = atomCount_;
    const uint32_t cellCount = cellCount_;

    // Один общий compute pass: последовательные диспатчи наблюдают storage-записи
    // друг друга (WebGPU вставляет неявный барьер между диспатчами одного пасса —
    // та же гарантия, на которой держится GpuResidentPhysics::step). Поэтому
    // НЕ нужен ни spin-wait, ни cross-workgroup sync — только порядок диспатчей.

    // 1) clear_counts (n = cellCount)
    {
        uint32_t off = slotOffset(cellCount);
        wgpu::BindGroup bg = makeClearBG();
        pass.setBindGroup(0, bg, 1, &off);
        pass.setPipeline(*clearPipeline_);
        pass.dispatchWorkgroups(ceilDiv(cellCount, 256u), 1, 1);
    }

    // 2) count_cells (n = atomCount; ядро гейтит по atomCount)
    if (n > 0u) {
        uint32_t off = slotOffset(atomCount_);
        wgpu::BindGroup bg = makeCountBG();
        pass.setBindGroup(0, bg, 1, &off);
        pass.setPipeline(*countPipeline_);
        pass.dispatchWorkgroups(ceilDiv(n, 256u), 1, 1);
    }

    // 3) exclusive scan cellCounts -> cellOffsets
    encodeScanExclusive(pass, *cellCounts_, cellCount, *cellOffsets_, 0);

    // 4) copy_offsets_to_cursors (n = cellCount)
    {
        uint32_t off = slotOffset(cellCount);
        wgpu::BindGroup bg = makeCopyBG();
        pass.setBindGroup(0, bg, 1, &off);
        pass.setPipeline(*copyCursorsPipeline_);
        pass.dispatchWorkgroups(ceilDiv(cellCount, 256u), 1, 1);
    }

    // 5) scatter_atoms (n = atomCount)
    if (n > 0u) {
        uint32_t off = slotOffset(atomCount_);
        wgpu::BindGroup bg = makeScatterBG();
        pass.setBindGroup(0, bg, 1, &off);
        pass.setPipeline(*scatterPipeline_);
        pass.dispatchWorkgroups(ceilDiv(n, 256u), 1, 1);
    }
}

void GpuNeighborListBuilder::buildNeighborListFull(const std::vector<float>& posX, const std::vector<float>& posY,
                                                   const std::vector<float>& posZ, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ,
                                                   float cellSize, uint32_t cellCount, float listRadiusSqr) {
    ensureInitialized();

    const uint32_t n = static_cast<uint32_t>(posX.size());
    if (posY.size() != n || posZ.size() != n) {
        throw std::runtime_error("GpuNeighborListBuilder::buildNeighborListFull: pos arrays length mismatch");
    }
    if (static_cast<uint64_t>(sizeX) * sizeY * sizeZ != cellCount) {
        throw std::runtime_error("GpuNeighborListBuilder::buildNeighborListFull: cellCount != sizeX*sizeY*sizeZ");
    }

    atomCount_ = n;
    cellCount_ = cellCount;
    sizeX_ = sizeX;
    sizeY_ = sizeY;
    sizeZ_ = sizeZ;
    cellSize_ = cellSize;
    listRadiusSqr_ = listRadiusSqr;

    ensureCapacity(n, cellCount);
    prepareSlots(); // слоты cell-list И NL-scan; Params несёт listRadiusSqr_/cellCount_

    uploadPositions(posX, posY, posZ);

    buildNeighborListFullCore();
}

void GpuNeighborListBuilder::buildNeighborListFullFromGpuPositions(wgpu::Buffer srcPositions, uint32_t atomCount, uint32_t sizeX,
                                                                   uint32_t sizeY, uint32_t sizeZ, float cellSize,
                                                                   uint32_t cellCount, float listRadiusSqr) {
    ensureInitialized();

    if (static_cast<uint64_t>(sizeX) * sizeY * sizeZ != cellCount) {
        throw std::runtime_error("GpuNeighborListBuilder::buildNeighborListFullFromGpuPositions: cellCount != sizeX*sizeY*sizeZ");
    }
    if (srcPositions == nullptr) {
        throw std::runtime_error("GpuNeighborListBuilder::buildNeighborListFullFromGpuPositions: null srcPositions");
    }

    atomCount_ = atomCount;
    cellCount_ = cellCount;
    sizeX_ = sizeX;
    sizeY_ = sizeY;
    sizeZ_ = sizeZ;
    cellSize_ = cellSize;
    listRadiusSqr_ = listRadiusSqr;

    ensureCapacity(atomCount, cellCount); // positions_ существует под atomCapacity_ до copy

    // GPU->GPU copy резидентных позиций в свой positions_ ВМЕСТО CPU-аплоада: смысл
    // резидентного режима — позиции не покидают VRAM. atomCount*16 байт (vec4 AoS),
    // src обязан иметь CopySrc (резидентный positions_ создан с stSrc).
    if (atomCount > 0u) {
        wgpu::Device dev = *WGPUContext::instance().device();
        wgpu::CommandEncoder enc = dev.createCommandEncoder({});
        enc.copyBufferToBuffer(srcPositions, 0, *positions_, 0, static_cast<uint64_t>(atomCount) * 16);
        wgpu::CommandBuffer cmd = enc.finish({});
        WGPUContext::instance().queue()->submit(1, &cmd);
    }

    prepareSlots(); // слоты cell-list И NL-scan; Params несёт listRadiusSqr_/cellCount_

    buildNeighborListFullCore();
}

void GpuNeighborListBuilder::buildNeighborListFullCore() {
    const uint32_t n = atomCount_;
    const uint32_t cellCount = cellCount_;
    (void)cellCount;

    wgpu::Device dev = *WGPUContext::instance().device();
    auto q = WGPUContext::instance().queue();

    if (n == 0u) {
        // Пустая сцена: cell-list всё равно нужен (clear/scan на cellCount), NL пуст.
        // nlOffsets[0]=0, total=0 — выставляем явно, чтобы readback был валиден.
        lastTotalNeighbors_ = 0u;
        const uint32_t zero = 0u;
        q->writeBuffer(*nlOffsets_, 0, &zero, sizeof(zero));
        wgpu::CommandEncoder enc = dev.createCommandEncoder({});
        wgpu::ComputePassEncoder pass = enc.beginComputePass({});
        encodeCellList(pass);
        pass.end();
        wgpu::CommandBuffer cmd = enc.finish({});
        q->submit(1, &cmd);
        dev.poll(true, nullptr);
        return;
    }

    // nlOffsets сканирует atomCount+1 элементов: neighborCounts[atomCount] должен
    // быть 0 (хвостовой слот), тогда nlOffsets[atomCount] == total соседей.
    // writeBuffer применяется ДО исполнения cmd, count_neighbors_full пишет лишь
    // [0,atomCount) — хвостовой 0 переживает проход.
    const uint32_t zero = 0u;
    q->writeBuffer(*neighborCounts_, static_cast<uint64_t>(n) * 4, &zero, sizeof(zero));

    // --- Submit A: cell-list + count_neighbors_full + scan -> nlOffsets ---
    {
        wgpu::CommandEncoder enc = dev.createCommandEncoder({});
        wgpu::ComputePassEncoder pass = enc.beginComputePass({});
        encodeCellList(pass);

        // count_neighbors_full (n = atomCount)
        {
            uint32_t off = slotOffset(atomCount_);
            wgpu::BindGroup bg = makeNlCountBG();
            pass.setBindGroup(0, bg, 1, &off);
            pass.setPipeline(*nlCountPipeline_);
            pass.dispatchWorkgroups(ceilDiv(n, 256u), 1, 1);
        }

        // exclusive scan neighborCounts(atomCount+1) -> nlOffsets
        encodeScanExclusive(pass, *neighborCounts_, n + 1u, *nlOffsets_, 0);

        pass.end();
        wgpu::CommandBuffer cmd = enc.finish({});
        q->submit(1, &cmd);
        dev.poll(true, nullptr);
    }

    // Скалярный readback total = nlOffsets[atomCount]: ровно 4 байта со смещения
    // atomCount (НЕ весь диапазон offsets). Это НЕ старый rebuild-round-trip (нет
    // скачивания позиций/CPU pair loop/upload NL): одно значение, чтобы выделить
    // точный nlNeighbors и не угадывать capacity (overflow — 2c).
    const uint32_t total = readU32(nlOffsets_, 1u, n)[0];
    lastTotalNeighbors_ = total; // GPU-доступ владельцу (totalNeighbors()) без повторного readback

    // Выделяем nlNeighbors под точный total (с запасом для роста между сценами).
    const wgpu::BufferUsage st = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    const uint32_t neighborsNeed = std::max<uint32_t>(total, 1u);
    if (neighborsNeed > nlNeighborsCapacity_) {
        const uint32_t cap = headroom(neighborsNeed);
        nlNeighbors_ = WGPUContext::instance().createBuffer(static_cast<size_t>(cap) * 4, st, "GNL_NlNeighbors");
        nlNeighborsCapacity_ = cap;
    }
    // Readback-буфер должен вмещать total для readbackNlNeighbors.
    if (total > readbackCapacity_) {
        readback_ = WGPUContext::instance().createBuffer(static_cast<size_t>(total) * 4,
                                                         wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst, "GNL_Readback");
        readbackCapacity_ = total;
    }

    // --- Submit B: write_neighbors_full (без атомиков, по слайсам nlOffsets) ---
    if (total > 0u) {
        wgpu::CommandEncoder enc = dev.createCommandEncoder({});
        wgpu::ComputePassEncoder pass = enc.beginComputePass({});
        uint32_t off = slotOffset(atomCount_);
        wgpu::BindGroup bg = makeNlWriteBG();
        pass.setBindGroup(0, bg, 1, &off);
        pass.setPipeline(*nlWritePipeline_);
        pass.dispatchWorkgroups(ceilDiv(n, 256u), 1, 1);
        pass.end();
        wgpu::CommandBuffer cmd = enc.finish({});
        q->submit(1, &cmd);
        dev.poll(true, nullptr);
    }
}

std::vector<uint32_t> GpuNeighborListBuilder::readU32(const wgpu::raii::Buffer& src, uint32_t count, uint32_t srcOffsetElems) const {
    std::vector<uint32_t> out(count);
    if (count == 0u) {
        return out;
    }
    const uint64_t bytes = static_cast<uint64_t>(count) * 4;

    wgpu::Device dev = *WGPUContext::instance().device();
    wgpu::CommandEncoder enc = dev.createCommandEncoder({});
    enc.copyBufferToBuffer(*src, static_cast<uint64_t>(srcOffsetElems) * 4, *readback_, 0, bytes);
    wgpu::CommandBuffer cmd = enc.finish({});
    WGPUContext::instance().queue()->submit(1, &cmd);

    struct MapCtx {
        bool done;
        bool ok;
    } ctx{false, true};
    auto cb = [](WGPUMapAsyncStatus s, WGPUStringView, void* u1, void*) {
        auto* c = static_cast<MapCtx*>(u1);
        c->ok = (s == WGPUMapAsyncStatus_Success);
        c->done = true;
    };
    wgpu::BufferMapCallbackInfo ci{};
    ci.mode = wgpu::CallbackMode::AllowSpontaneous;
    ci.callback = cb;
    ci.userdata1 = &ctx;
    readback_->mapAsync(wgpu::MapMode::Read, 0, bytes, ci);
    while (!ctx.done) {
        dev.poll(true, nullptr);
    }
    if (!ctx.ok) {
        throw std::runtime_error("GpuNeighborListBuilder: readback map failed");
    }
    const uint32_t* data = static_cast<const uint32_t*>(readback_->getConstMappedRange(0, bytes));
    std::memcpy(out.data(), data, bytes);
    readback_->unmap();
    return out;
}

std::vector<uint32_t> GpuNeighborListBuilder::readbackCellOffsets() const { return readU32(cellOffsets_, cellCount_); }
std::vector<uint32_t> GpuNeighborListBuilder::readbackCellCounts() const { return readU32(cellCounts_, cellCount_); }
std::vector<uint32_t> GpuNeighborListBuilder::readbackAtomsInCells() const { return readU32(atomsInCells_, atomCount_); }

// nlOffsets длиной atomCount+1 (как CPU offsets_). readback_ вмещает atomCount+1
// (>= после ensureCapacity). При atomCount==0 возвращаем [0] вручную (readU32 на 1).
std::vector<uint32_t> GpuNeighborListBuilder::readbackNlOffsets() const { return readU32(nlOffsets_, atomCount_ + 1u); }
std::vector<uint32_t> GpuNeighborListBuilder::readbackNeighborCounts() const { return readU32(neighborCounts_, atomCount_); }

std::vector<uint32_t> GpuNeighborListBuilder::readbackNlNeighbors(uint32_t total) const {
    if (total == 0u) {
        return {};
    }
    return readU32(nlNeighbors_, total);
}
