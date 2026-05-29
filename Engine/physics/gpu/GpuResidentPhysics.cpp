#include "GpuResidentPhysics.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/gpu/GpuNeighborListBuilder.h"
#include "Rendering/WGPUContext.h"

#include "generated/shaders/integrate_verlet.wgsl.h"
#include "generated/shaders/nl_displacement.wgsl.h"
#include "generated/shaders/physics_lj.wgsl.h"

namespace {

struct LJUniforms {
    float cutoffSqr;
    float epsilon;
    uint32_t mobileCount;
    uint32_t typeCount;
};

struct IntegratorUniforms {
    float dt;
    float accelDamping;
    float worldMaxX, worldMaxY, worldMaxZ;
    float restitution;
    uint32_t mobileCount;
    uint32_t totalCount;
};

struct DispUniforms {
    uint32_t mobileCount;
};

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

constexpr size_t kHeadroom(size_t n) { return n + n / 2 + 1; }

} // namespace

GpuResidentPhysics::GpuResidentPhysics() = default;

GpuResidentPhysics::~GpuResidentPhysics() {
    // Pending async map не должен пережить объект: его callback (произвольный
    // поток) пишет в член dispMap_, а буфер dispReadback_ освобождается ниже
    // member-деструкторами. Дренируем здесь (singleton WGPUContext переживает
    // нас, device ещё валиден при рантайм-сносе мира removeWorld). Иначе callback
    // ударил бы по освобождённой памяти.
    if (dispCheckPending_ && initialized_) {
        discardPendingDisplacementCheck();
    }
}

void GpuResidentPhysics::ensureInitialized() {
    if (initialized_) {
        return;
    }
    wgpu::Device dev = *WGPUContext::instance().device();
    if (dev == nullptr) {
        throw std::runtime_error("GpuResidentPhysics: WGPUContext device not initialized");
    }

    // LJ layout (7): uniform + pos/types/off/nbr/ljPairs(read) + forces(rw)
    {
        std::array<wgpu::BindGroupLayoutEntry, 7> e{};
        e[0].binding = 0;
        e[0].visibility = wgpu::ShaderStage::Compute;
        e[0].buffer.type = wgpu::BufferBindingType::Uniform;
        for (uint32_t i = 1; i <= 5; ++i) {
            e[i].binding = i;
            e[i].visibility = wgpu::ShaderStage::Compute;
            e[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        }
        e[6].binding = 6;
        e[6].visibility = wgpu::ShaderStage::Compute;
        e[6].buffer.type = wgpu::BufferBindingType::Storage;
        ljLayout_ = WGPUContext::instance().createBindGroupLayout(e, "GRP_LJ_BGL");
    }
    // Integrator layout (6): uniform + pos/vel/forces(rw) + prevForces/invMass(read)
    {
        std::array<wgpu::BindGroupLayoutEntry, 6> e{};
        e[0].binding = 0;
        e[0].visibility = wgpu::ShaderStage::Compute;
        e[0].buffer.type = wgpu::BufferBindingType::Uniform;
        e[1].binding = 1;
        e[1].visibility = wgpu::ShaderStage::Compute;
        e[1].buffer.type = wgpu::BufferBindingType::Storage;
        e[2].binding = 2;
        e[2].visibility = wgpu::ShaderStage::Compute;
        e[2].buffer.type = wgpu::BufferBindingType::Storage;
        e[3].binding = 3;
        e[3].visibility = wgpu::ShaderStage::Compute;
        e[3].buffer.type = wgpu::BufferBindingType::Storage;
        e[4].binding = 4;
        e[4].visibility = wgpu::ShaderStage::Compute;
        e[4].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        e[5].binding = 5;
        e[5].visibility = wgpu::ShaderStage::Compute;
        e[5].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        intLayout_ = WGPUContext::instance().createBindGroupLayout(e, "GRP_Int_BGL");
    }
    // Displacement layout (4): uniform + pos/refPos(read) + flag(rw atomic)
    {
        std::array<wgpu::BindGroupLayoutEntry, 4> e{};
        e[0].binding = 0;
        e[0].visibility = wgpu::ShaderStage::Compute;
        e[0].buffer.type = wgpu::BufferBindingType::Uniform;
        e[1].binding = 1;
        e[1].visibility = wgpu::ShaderStage::Compute;
        e[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        e[2].binding = 2;
        e[2].visibility = wgpu::ShaderStage::Compute;
        e[2].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        e[3].binding = 3;
        e[3].visibility = wgpu::ShaderStage::Compute;
        e[3].buffer.type = wgpu::BufferBindingType::Storage;
        dispLayout_ = WGPUContext::instance().createBindGroupLayout(e, "GRP_Disp_BGL");
    }

    wgpu::ShaderModule ljMod = makeModule(physics_ljWGSL);
    ljPipeline_ = makePipeline(*ljLayout_, ljMod, "compute_lj");
    wgpu::ShaderModule iMod = makeModule(integrate_verletWGSL);
    predictPipeline_ = makePipeline(*intLayout_, iMod, "predict");
    confinePipeline_ = makePipeline(*intLayout_, iMod, "confine");
    zeroPipeline_ = makePipeline(*intLayout_, iMod, "zero_forces");
    correctPipeline_ = makePipeline(*intLayout_, iMod, "correct");
    wgpu::ShaderModule dMod = makeModule(nl_displacementWGSL);
    displacementPipeline_ = makePipeline(*dispLayout_, dMod, "max_displacement");

    ljUniform_ = WGPUContext::instance().createBuffer(sizeof(LJUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                      "GRP_LJU");
    intUniform_ = WGPUContext::instance().createBuffer(sizeof(IntegratorUniforms),
                                                       wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "GRP_IntU");
    dispUniform_ = WGPUContext::instance().createBuffer(sizeof(DispUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                        "GRP_DispU");
    dispFlag_ = WGPUContext::instance().createBuffer(sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst |
                                                                           wgpu::BufferUsage::CopySrc,
                                                     "GRP_DispFlag");
    dispReadback_ = WGPUContext::instance().createBuffer(sizeof(uint32_t), wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                                         "GRP_DispReadback");

    // ljPairs_ фиксированного размера (TypeCount^2 × vec2) — создаём здесь, до
    // первого rebuildBindGroups (он ссылается на этот буфер). Данные зальём
    // в uploadFromCpu один раз.
    constexpr size_t kTC = static_cast<size_t>(AtomData::Type::COUNT);
    ljPairs_ = WGPUContext::instance().createBuffer(kTC * kTC * 2 * sizeof(float),
                                                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, "GRP_LJPairs");

    initialized_ = true;
}

void GpuResidentPhysics::ensureCapacity(size_t totalCount, size_t mobileCount, size_t neighborCount) {
    (void)mobileCount;
    // rebuildBindGroups биндит ВСЕ буфера безусловно, поэтому даже на пустой
    // сцене (0 атомов / 0 соседей — напр. свежесозданный мир) они должны
    // существовать: иначе createBindGroup получит null-ресурс и wgpu панично
    // падает ("invalid bind group entry"). Резервируем минимум 1 элемент; шаг
    // всё равно диспатчит 0 рабочих групп при 0 мобильных атомах.
    totalCount = std::max<size_t>(totalCount, 1);
    neighborCount = std::max<size_t>(neighborCount, 1);

    const wgpu::BufferUsage st = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    const wgpu::BufferUsage stSrc = st | wgpu::BufferUsage::CopySrc;

    bool grew = false;
    if (totalCount > atomCapacity_) {
        const size_t cap = kHeadroom(totalCount);
        positions_ = WGPUContext::instance().createBuffer(cap * 16, stSrc, "GRP_Pos");
        velocities_ = WGPUContext::instance().createBuffer(cap * 16, stSrc, "GRP_Vel");
        forces_[0] = WGPUContext::instance().createBuffer(cap * 16, st, "GRP_F0");
        forces_[1] = WGPUContext::instance().createBuffer(cap * 16, st, "GRP_F1");
        invMass_ = WGPUContext::instance().createBuffer(cap * 4, st, "GRP_InvMass");
        types_ = WGPUContext::instance().createBuffer(cap * 4, st, "GRP_Types");
        refPos_ = WGPUContext::instance().createBuffer(cap * 16, st, "GRP_RefPos");
        posReadback_ = WGPUContext::instance().createBuffer(cap * 16, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                                            "GRP_PosReadback");
        velReadback_ = WGPUContext::instance().createBuffer(cap * 16, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                                            "GRP_VelReadback");
        atomCapacity_ = cap;
        grew = true;
    }
    if (neighborCount > nlNeighborsCapacity_) {
        const size_t cap = kHeadroom(neighborCount);
        // stSrc: CopyDst (writeBuffer/GPU->GPU copy назначение) + CopySrc (2c readback
        // и любой будущий GPU->GPU consumer резидентного NL). CopySrc на read-only-в-
        // шейдере буфере безвреден — это лишь usage-флаг, не меняет доступ LJ-ядра.
        nlNeighbors_ = WGPUContext::instance().createBuffer(cap * 4, stSrc, "GRP_Nbr");
        nlNeighborsCapacity_ = cap;
        grew = true;
    }
    if (totalCount + 1 > nlOffsetsCapacity_) {
        const size_t cap = kHeadroom(totalCount + 1);
        nlOffsets_ = WGPUContext::instance().createBuffer(cap * 4, stSrc, "GRP_Off");
        nlOffsetsCapacity_ = cap;
        grew = true;
    }
    if (grew) {
        rebuildBindGroups();
    }
}

void GpuResidentPhysics::rebuildBindGroups() {
    const uint64_t vec4Bytes = atomCapacity_ * 16;
    const uint64_t f32Bytes = atomCapacity_ * 4;

    constexpr size_t kTC = static_cast<size_t>(AtomData::Type::COUNT);
    const uint64_t ljPairsBytes = kTC * kTC * 2 * sizeof(float);

    // LJ bind groups (forces -> forces_[p]) для p=0,1.
    for (int p = 0; p < 2; ++p) {
        std::array<wgpu::BindGroupEntry, 7> b{};
        b[0].binding = 0;
        b[0].buffer = *ljUniform_;
        b[0].size = sizeof(LJUniforms);
        b[1].binding = 1;
        b[1].buffer = *positions_;
        b[1].size = vec4Bytes;
        b[2].binding = 2;
        b[2].buffer = *types_;
        b[2].size = f32Bytes;
        b[3].binding = 3;
        b[3].buffer = *nlOffsets_;
        b[3].size = nlOffsetsCapacity_ * 4;
        b[4].binding = 4;
        b[4].buffer = *nlNeighbors_;
        b[4].size = nlNeighborsCapacity_ * 4;
        b[5].binding = 5;
        b[5].buffer = *ljPairs_;
        b[5].size = ljPairsBytes;
        b[6].binding = 6;
        b[6].buffer = *forces_[p];
        b[6].size = vec4Bytes;
        ljBindGroup_[p] = WGPUContext::instance().createBindGroup(*ljLayout_, b, "GRP_LJ_BG");
    }

    // Integrator bind groups: forces=forces_[p], prevForces=forces_[1-p].
    for (int p = 0; p < 2; ++p) {
        std::array<wgpu::BindGroupEntry, 6> b{};
        b[0].binding = 0;
        b[0].buffer = *intUniform_;
        b[0].size = sizeof(IntegratorUniforms);
        b[1].binding = 1;
        b[1].buffer = *positions_;
        b[1].size = vec4Bytes;
        b[2].binding = 2;
        b[2].buffer = *velocities_;
        b[2].size = vec4Bytes;
        b[3].binding = 3;
        b[3].buffer = *forces_[p];
        b[3].size = vec4Bytes;
        b[4].binding = 4;
        b[4].buffer = *forces_[1 - p];
        b[4].size = vec4Bytes;
        b[5].binding = 5;
        b[5].buffer = *invMass_;
        b[5].size = f32Bytes;
        intBindGroup_[p] = WGPUContext::instance().createBindGroup(*intLayout_, b, "GRP_Int_BG");
    }

    // Displacement bind group.
    {
        std::array<wgpu::BindGroupEntry, 4> b{};
        b[0].binding = 0;
        b[0].buffer = *dispUniform_;
        b[0].size = sizeof(DispUniforms);
        b[1].binding = 1;
        b[1].buffer = *positions_;
        b[1].size = vec4Bytes;
        b[2].binding = 2;
        b[2].buffer = *refPos_;
        b[2].size = vec4Bytes;
        b[3].binding = 3;
        b[3].buffer = *dispFlag_;
        b[3].size = sizeof(uint32_t);
        dispBindGroup_ = WGPUContext::instance().createBindGroup(*dispLayout_, b, "GRP_Disp_BG");
    }
}

void GpuResidentPhysics::uploadFromCpu(const AtomStorage& atoms, const NeighborList& neighborList, const LJForceField& ljForceField,
                                       float worldSizeX, float worldSizeY, float worldSizeZ) {
    ensureInitialized();

    const size_t n = atoms.size();
    const auto& offsets = neighborList.offsets();
    const auto& neighbors = neighborList.neighbors();
    ensureCapacity(n, atoms.mobileCount(), neighbors.size());

    mobileCount_ = static_cast<uint32_t>(atoms.mobileCount());
    totalCount_ = static_cast<uint32_t>(n);
    cutoffSqr_ = neighborList.cutoff() * neighborList.cutoff();
    worldMax_[0] = worldSizeX - 1.0f;
    worldMax_[1] = worldSizeY - 1.0f;
    worldMax_[2] = worldSizeZ - 1.0f;
    parity_ = 0;

    std::vector<float> pos(n * 4), vel(n * 4), im(n);
    std::vector<uint32_t> ty(n);
    for (size_t i = 0; i < n; ++i) {
        pos[i * 4 + 0] = atoms.posX(i);
        pos[i * 4 + 1] = atoms.posY(i);
        pos[i * 4 + 2] = atoms.posZ(i);
        pos[i * 4 + 3] = 0.0f;
        vel[i * 4 + 0] = atoms.velX(i);
        vel[i * 4 + 1] = atoms.velY(i);
        vel[i * 4 + 2] = atoms.velZ(i);
        vel[i * 4 + 3] = 0.0f;
        im[i] = atoms.invMass(i);
        ty[i] = static_cast<uint32_t>(atoms.type(i));
    }

    auto q = WGPUContext::instance().queue();
    q->writeBuffer(*positions_, 0, pos.data(), pos.size() * 4);
    q->writeBuffer(*velocities_, 0, vel.data(), vel.size() * 4);
    q->writeBuffer(*invMass_, 0, im.data(), im.size() * 4);
    q->writeBuffer(*types_, 0, ty.data(), ty.size() * 4);
    // forces начинаем с нуля — первый шаг predict сдвинет по нулевой силе,
    // как CPU при свежей сцене (forces инициализированы нулями в AtomStorage).
    std::vector<float> zero(n * 4, 0.0f);
    q->writeBuffer(*forces_[0], 0, zero.data(), zero.size() * 4);
    q->writeBuffer(*forces_[1], 0, zero.data(), zero.size() * 4);

    if (!ljTableUploaded_) {
        constexpr size_t kTC = static_cast<size_t>(AtomData::Type::COUNT);
        std::vector<float> packed(kTC * kTC * 2);
        for (size_t i = 0; i < kTC; ++i) {
            const auto& row = ljForceField.pairRow(static_cast<AtomData::Type>(i));
            for (size_t j = 0; j < kTC; ++j) {
                packed[(i * kTC + j) * 2 + 0] = row[j].potentialC6;
                packed[(i * kTC + j) * 2 + 1] = row[j].potentialC12;
            }
        }
        ljPairs_ = WGPUContext::instance().createBuffer(packed.size() * 4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst,
                                                        "GRP_LJPairs");
        q->writeBuffer(*ljPairs_, 0, packed.data(), packed.size() * 4);
        ljTableUploaded_ = true;
        rebuildBindGroups(); // ljPairs_ только что создан — перепривязать
    }

    uploadNeighborList(neighborList);

    LJUniforms lju{cutoffSqr_, 1e-6f, mobileCount_, static_cast<uint32_t>(AtomData::Type::COUNT)};
    q->writeBuffer(*ljUniform_, 0, &lju, sizeof(lju));
    DispUniforms du{mobileCount_};
    q->writeBuffer(*dispUniform_, 0, &du, sizeof(du));
}

void GpuResidentPhysics::uploadNeighborList(const NeighborList& neighborList) {
    // Этот метод перезаписывает refPos (база displacement-проверки), поэтому любой
    // pending async disp-check против старого refPos становится недействителен.
    // Сносим его здесь, в самом владельце refPos — не полагаемся на вызывающего.
    discardPendingDisplacementCheck();

    const auto& offsets = neighborList.offsets();
    const auto& neighbors = neighborList.neighbors();
    ensureCapacity(totalCount_, mobileCount_, neighbors.size());

    auto q = WGPUContext::instance().queue();
    q->writeBuffer(*nlOffsets_, 0, offsets.data(), offsets.size() * 4);
    if (!neighbors.empty()) {
        q->writeBuffer(*nlNeighbors_, 0, neighbors.data(), neighbors.size() * 4);
    }
    // refPos := текущие позиции (копия GPU->GPU): база для displacement-проверки.
    wgpu::Device dev = *WGPUContext::instance().device();
    wgpu::CommandEncoder enc = dev.createCommandEncoder({});
    enc.copyBufferToBuffer(*positions_, 0, *refPos_, 0, static_cast<uint64_t>(totalCount_) * 16);
    wgpu::CommandBuffer cmd = enc.finish({});
    q->submit(1, &cmd);
}

void GpuResidentPhysics::rebuildNeighborListOnGpu(uint32_t gridSizeX, uint32_t gridSizeY, uint32_t gridSizeZ, float cellSize,
                                                  uint32_t cellCount, float listRadiusSqr) {
    ensureInitialized(); // positions_ и резидентные NL-буфера должны существовать
    ++nlRebuilds_;       // 2e: телеметрия — по разу на каждый вызов (GPU-аналог CPU rebuildCount)

    // Перезаписываем refPos (как uploadNeighborList) — любой pending async disp-check
    // против старого refPos недействителен. Сносим здесь, в самом владельце refPos.
    discardPendingDisplacementCheck();

    // Lazy-инициализация внутреннего builder'а (резидентные инстансы без GPU-rebuild
    // не платят за его буфера).
    if (!nlBuilder_) {
        nlBuilder_ = std::make_unique<GpuNeighborListBuilder>();
    }

    // Строим Full NL ЦЕЛИКОМ на GPU. Позиции НЕ качаем в CPU: builder берёт их
    // GPU->GPU из резидентного positions_. Блокирующе — по возврату shadow-буфера
    // builder'а (nlOffsets/nlNeighbors) валидны и totalNeighbors() точен.
    nlBuilder_->buildNeighborListFullFromGpuPositions(*positions_, totalCount_, gridSizeX, gridSizeY, gridSizeZ, cellSize,
                                                      cellCount, listRadiusSqr);

    const uint32_t total = nlBuilder_->totalNeighbors();

    // --- Overflow fail-closed ---
    // total известен после count+scan. Если он превышает резидентную ёмкость
    // nlNeighbors_ — РАСТИМ её ДО copy (ensureCapacity пересоздаёт буфер с запасом
    // и rebuildBindGroups перепривязывает LJ-группу на новый буфер). Builder уже
    // выделил свой nlNeighbors под точный total и write_neighbors_full записал ВСЕ
    // пары — поэтому после роста резидентного буфера до >= total копия несёт полный
    // NL. Частичный/усечённый NL в LJ-ядро не попадает: рост происходит здесь, а
    // LJ читает резидентный буфер только в step() (не вызывается из 2c).
    if (static_cast<size_t>(total) > nlNeighborsCapacity_) {
        ensureCapacity(totalCount_, mobileCount_, total);
        ++nlCapacityGrows_;
    }

    // --- GPU->GPU copy shadow NL -> резидентные nlOffsets_/nlNeighbors_ ---
    // offsets: totalCount_+1 элементов (CSR, [totalCount_]=total). neighbors: total.
    // Оба буфера builder'а имеют CopySrc; резидентные — CopyDst.
    wgpu::Device dev = *WGPUContext::instance().device();
    auto q = WGPUContext::instance().queue();
    {
        wgpu::CommandEncoder enc = dev.createCommandEncoder({});
        enc.copyBufferToBuffer(nlBuilder_->nlOffsetsBuffer(), 0, *nlOffsets_, 0, (static_cast<uint64_t>(totalCount_) + 1u) * 4);
        if (total > 0u) {
            enc.copyBufferToBuffer(nlBuilder_->nlNeighborsBuffer(), 0, *nlNeighbors_, 0, static_cast<uint64_t>(total) * 4);
        }
        // refPos := текущие позиции (как uploadNeighborList): база displacement-проверки.
        if (totalCount_ > 0u) {
            enc.copyBufferToBuffer(*positions_, 0, *refPos_, 0, static_cast<uint64_t>(totalCount_) * 16);
        }
        wgpu::CommandBuffer cmd = enc.finish({});
        q->submit(1, &cmd);
    }
}

namespace {

// Блокирующий readback u32-буфера (bench/диагностика). Копирует count элементов
// со смещения srcOffsetElems в свежий MapRead-буфер, дренит очередь, возвращает
// вектор. Не для hot loop — временный буфер на каждый вызов (редкая sync-точка).
std::vector<uint32_t> readU32Blocking(const wgpu::raii::Buffer& src, uint32_t count, uint32_t srcOffsetElems) {
    std::vector<uint32_t> out(count);
    if (count == 0u) {
        return out;
    }
    const uint64_t bytes = static_cast<uint64_t>(count) * 4;
    wgpu::Device dev = *WGPUContext::instance().device();
    wgpu::Buffer rb = WGPUContext::instance().createBuffer(bytes, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                                           "GRP_NlReadback");

    wgpu::CommandEncoder enc = dev.createCommandEncoder({});
    enc.copyBufferToBuffer(*src, static_cast<uint64_t>(srcOffsetElems) * 4, rb, 0, bytes);
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
    rb.mapAsync(wgpu::MapMode::Read, 0, bytes, ci);
    while (!ctx.done) {
        dev.poll(true, nullptr);
    }
    if (!ctx.ok) {
        throw std::runtime_error("GpuResidentPhysics: NL readback map failed");
    }
    const uint32_t* data = static_cast<const uint32_t*>(rb.getConstMappedRange(0, bytes));
    std::memcpy(out.data(), data, bytes);
    rb.unmap();
    return out;
}

} // namespace

std::vector<uint32_t> GpuResidentPhysics::readbackNlOffsets() const {
    return readU32Blocking(nlOffsets_, totalCount_ + 1u, 0u);
}

std::vector<uint32_t> GpuResidentPhysics::readbackNlNeighbors(uint32_t total) const {
    if (total == 0u) {
        return {};
    }
    return readU32Blocking(nlNeighbors_, total, 0u);
}

void GpuResidentPhysics::step(float dt, float accelDamping) {
    IntegratorUniforms iu{dt, accelDamping, worldMax_[0], worldMax_[1], worldMax_[2], 0.8f, mobileCount_, totalCount_};
    WGPUContext::instance().queue()->writeBuffer(*intUniform_, 0, &iu, sizeof(iu));

    wgpu::Device dev = *WGPUContext::instance().device();
    wgpu::CommandEncoder enc = dev.createCommandEncoder({});
    wgpu::ComputePassEncoder pass = enc.beginComputePass({});
    const uint32_t gMobile = (mobileCount_ + 63u) / 64u;
    const uint32_t gTotal = (totalCount_ + 63u) / 64u;
    const int p = parity_;        // in = forces_[p] (сила прошлого шага)
    const int out = 1 - p;        // out = forces_[1-p] (сила этого шага)

    // predict читает текущую силу = in = forces_[p] => intBindGroup_[p] (forces=forces_[p])
    pass.setBindGroup(0, *intBindGroup_[p], 0, nullptr);
    pass.setPipeline(*predictPipeline_);
    pass.dispatchWorkgroups(gMobile, 1, 1);
    pass.setPipeline(*confinePipeline_);
    pass.dispatchWorkgroups(gMobile, 1, 1);

    // zero + LJ пишут out = forces_[1-p]; correct читает out + in.
    // intBindGroup_[out] имеет forces=forces_[out], prevForces=forces_[p]=in.
    pass.setBindGroup(0, *intBindGroup_[out], 0, nullptr);
    pass.setPipeline(*zeroPipeline_);
    pass.dispatchWorkgroups(gTotal, 1, 1);

    pass.setBindGroup(0, *ljBindGroup_[out], 0, nullptr);
    pass.setPipeline(*ljPipeline_);
    pass.dispatchWorkgroups(gMobile, 1, 1);

    pass.setBindGroup(0, *intBindGroup_[out], 0, nullptr);
    pass.setPipeline(*correctPipeline_);
    pass.dispatchWorkgroups(gMobile, 1, 1);

    pass.end();
    wgpu::CommandBuffer cmd = enc.finish({});
    WGPUContext::instance().queue()->submit(1, &cmd);

    parity_ = out; // следующий шаг: in = эта сила

    if (dispCheckPending_) {
        ++dispCheckAgeSteps_; // возраст async disp-check для hard backstop
    }
}

void GpuResidentPhysics::downloadToCpu(AtomStorage& atoms, bool withVelocities) {
    // Усечение CPU-сцены (removeAtom/clear) при активном GPU — реальный transient:
    // totalCount_ ещё старый, а AtomStorage уже меньше. Качаем min, иначе циклы
    // записи ниже выйдут за границы AtomStorage. Ближайший updateState сделает
    // re-upload и синхронизирует totalCount_.
    const size_t n = std::min<size_t>(static_cast<size_t>(totalCount_), atoms.size());
    if (n == 0) {
        return;
    }
    const uint64_t bytes = static_cast<uint64_t>(n) * 16;

    wgpu::Device dev = *WGPUContext::instance().device();
    wgpu::CommandEncoder enc = dev.createCommandEncoder({});
    enc.copyBufferToBuffer(*positions_, 0, *posReadback_, 0, bytes);
    if (withVelocities) {
        enc.copyBufferToBuffer(*velocities_, 0, *velReadback_, 0, bytes);
    }
    wgpu::CommandBuffer cmd = enc.finish({});
    WGPUContext::instance().queue()->submit(1, &cmd);

    struct MapCtx {
        int pending;
        bool ok;
    } ctx{withVelocities ? 2 : 1, true};
    auto cb = [](WGPUMapAsyncStatus s, WGPUStringView, void* u1, void*) {
        auto* c = static_cast<MapCtx*>(u1);
        --c->pending;
        if (s != WGPUMapAsyncStatus_Success) {
            c->ok = false;
        }
    };
    wgpu::BufferMapCallbackInfo ci{};
    ci.mode = wgpu::CallbackMode::AllowSpontaneous;
    ci.callback = cb;
    ci.userdata1 = &ctx;
    posReadback_->mapAsync(wgpu::MapMode::Read, 0, bytes, ci);
    if (withVelocities) {
        velReadback_->mapAsync(wgpu::MapMode::Read, 0, bytes, ci);
    }
    while (ctx.pending > 0) {
        dev.poll(true, nullptr);
    }
    if (!ctx.ok) {
        throw std::runtime_error("GpuResidentPhysics: download map failed");
    }

    const float* pos = static_cast<const float*>(posReadback_->getConstMappedRange(0, bytes));
    for (size_t i = 0; i < n; ++i) {
        atoms.posX(i) = pos[i * 4 + 0];
        atoms.posY(i) = pos[i * 4 + 1];
        atoms.posZ(i) = pos[i * 4 + 2];
    }
    posReadback_->unmap();

    if (withVelocities) {
        const float* vel = static_cast<const float*>(velReadback_->getConstMappedRange(0, bytes));
        for (size_t i = 0; i < n; ++i) {
            atoms.velX(i) = vel[i * 4 + 0];
            atoms.velY(i) = vel[i * 4 + 1];
            atoms.velZ(i) = vel[i * 4 + 2];
        }
        velReadback_->unmap();
    }
}

void GpuResidentPhysics::submitDisplacementReductionAndMap() {
    wgpu::Device dev = *WGPUContext::instance().device();
    // reset флага (CPU write 0), max_displacement kernel, copy в readback.
    const uint32_t zero = 0;
    WGPUContext::instance().queue()->writeBuffer(*dispFlag_, 0, &zero, sizeof(zero));

    wgpu::CommandEncoder enc = dev.createCommandEncoder({});
    wgpu::ComputePassEncoder pass = enc.beginComputePass({});
    pass.setBindGroup(0, *dispBindGroup_, 0, nullptr);
    pass.setPipeline(*displacementPipeline_);
    pass.dispatchWorkgroups((mobileCount_ + 63u) / 64u, 1, 1);
    pass.end();
    enc.copyBufferToBuffer(*dispFlag_, 0, *dispReadback_, 0, sizeof(uint32_t));
    wgpu::CommandBuffer cmd = enc.finish({});
    WGPUContext::instance().queue()->submit(1, &cmd);

    // mapAsync без блокировки. Callback может прийти с произвольного потока
    // (AllowSpontaneous) — пишем атомарно и больше ничего (без webgpu-вызовов).
    // ok store ДО done store: main-thread, увидев done==true, гарантированно видит
    // актуальный ok (seq_cst total order).
    dispMap_.done.store(false);
    dispMap_.ok.store(true);
    auto cb = [](WGPUMapAsyncStatus s, WGPUStringView, void* u1, void*) {
        auto* m = static_cast<DispMapState*>(u1);
        m->ok.store(s == WGPUMapAsyncStatus_Success);
        m->done.store(true);
    };
    wgpu::BufferMapCallbackInfo ci{};
    ci.mode = wgpu::CallbackMode::AllowSpontaneous;
    ci.callback = cb;
    ci.userdata1 = &dispMap_;
    dispReadback_->mapAsync(wgpu::MapMode::Read, 0, sizeof(uint32_t), ci);
}

float GpuResidentPhysics::readDisplacementResultAndClear() {
    if (!dispMap_.ok.load()) {
        dispCheckPending_ = false;
        throw std::runtime_error("GpuResidentPhysics: displacement map failed");
    }
    const uint32_t* bits = static_cast<const uint32_t*>(dispReadback_->getConstMappedRange(0, sizeof(uint32_t)));
    uint32_t maxBits = *bits;
    dispReadback_->unmap();
    dispCheckPending_ = false;
    float result;
    std::memcpy(&result, &maxBits, sizeof(result));
    return result;
}

void GpuResidentPhysics::beginMaxDisplacementSqrAsync() {
    if (dispCheckPending_) {
        return; // single-in-flight: предыдущий ещё не завершён
    }
    submitDisplacementReductionAndMap();
    dispCheckPending_ = true;
    dispCheckAgeSteps_ = 0;
    ++dispBeginCount_;
}

std::optional<float> GpuResidentPhysics::tryConsumeMaxDisplacementSqr() {
    if (!dispCheckPending_) {
        return std::nullopt;
    }
    wgpu::Device dev = *WGPUContext::instance().device();
    dev.poll(false, nullptr); // неблокирующе прокрутить spontaneous-callbacks
    if (!dispMap_.done.load()) {
        return std::nullopt; // результат ещё не готов — без столла
    }
    const float result = readDisplacementResultAndClear();
    ++dispConsumeCount_; // забрали async без столла
    return result;
}

float GpuResidentPhysics::finishMaxDisplacementSqrBlocking() {
    wgpu::Device dev = *WGPUContext::instance().device();
    while (!dispMap_.done.load()) {
        dev.poll(true, nullptr); // hard backstop: дождаться pending-результата
    }
    const float result = readDisplacementResultAndClear();
    ++dispBackstopCount_; // пришлось блокирующе дождаться
    return result;
}

void GpuResidentPhysics::discardPendingDisplacementCheck() {
    if (!dispCheckPending_) {
        return;
    }
    // Буфер mid-map после reupload (refPos устарел) / при сносе объекта: нельзя
    // бросить — дождаться callback, unmap, отбросить значение.
    wgpu::Device dev = *WGPUContext::instance().device();
    while (!dispMap_.done.load()) {
        dev.poll(true, nullptr);
    }
    if (dispMap_.ok.load()) {
        dispReadback_->unmap();
    }
    dispCheckPending_ = false;
    ++dispDiscardCount_;
}

float GpuResidentPhysics::maxDisplacementSqr() {
    // Блокирующий вариант (совместимость/тесты): отбросить pending async, запустить
    // свежую редукцию и дождаться. Дренит GPU-очередь — не для hot loop.
    discardPendingDisplacementCheck();
    submitDisplacementReductionAndMap();
    dispCheckPending_ = true;
    return finishMaxDisplacementSqrBlocking();
}
