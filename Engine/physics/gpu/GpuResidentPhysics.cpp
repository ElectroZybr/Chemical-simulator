#include "GpuResidentPhysics.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/ForceFields/CoulombForceField.h" // kCoulombEvAngstrom (== CPU-константа в uniform)
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/gpu/GpuNeighborListBuilder.h"
#include "Rendering/WGPUContext.h"

#include "generated/shaders/integrate_verlet.wgsl.h"
#include "generated/shaders/nl_displacement.wgsl.h"
#include "generated/shaders/physics_bond.wgsl.h"
#include "generated/shaders/physics_coulomb.wgsl.h"
#include "generated/shaders/physics_lj.wgsl.h"
#include "generated/shaders/physics_wall.wgsl.h"

namespace {

struct LJUniforms {
    float cutoffSqr;
    float epsilon;
    uint32_t mobileCount;
    uint32_t typeCount;
};

// Раскладка ОБЯЗАНА совпадать с CoulombUniforms в physics_coulomb.wgsl (тот же
// порядок полей). 16 байт (кратно 16 → uniform-выравнивание без хвостового
// паддинга). Если поле переставят/переименуют и размеры разойдутся, в
// mobileCount/kCoulomb попадут чужие байты → физика молча поедет (провал
// parity-гейта вдали от причины). Ловим на компиляции.
struct CoulombUniforms {
    float cutoffSqr;
    float epsilon;
    uint32_t mobileCount;
    float kCoulomb; // == CoulombForceField.h:14 (140.399645)
};
static_assert(sizeof(CoulombUniforms) == 16, "CoulombUniforms != 16 байт: разошёлся контракт с physics_coulomb.wgsl");

// Раскладка ОБЯЗАНА совпадать с WallUniforms в physics_wall.wgsl (тот же порядок
// полей + хвостовой паддинг до кратного 16 байт под uniform-binding).
struct WallUniforms {
    float worldMaxX, worldMaxY, worldMaxZ;
    float gravityX, gravityY, gravityZ;
    float k, border; // == WallForceField.cpp:28-29 (500.0, 2.0)
    uint32_t mobileCount;
    uint32_t pad0, pad1, pad2; // выравнивание до 48 байт (кратно 16)
};
// Контракт с physics_wall.wgsl держится вручную (C++ struct <-> WGSL struct под
// uniform-раскладкой). Если кто-то переставит/переименует поле и размеры разойдутся,
// в mobileCount/k попадут чужие байты — физика молча поедет (ни краша, ни ошибки
// компиляции, только провал parity-гейта вдали от причины). Ловим на компиляции.
static_assert(sizeof(WallUniforms) == 48, "WallUniforms != 48 байт: разошёлся контракт с physics_wall.wgsl");

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

// Раскладка ОБЯЗАНА совпадать с BondUniforms в physics_bond.wgsl (порядок полей +
// хвостовой паддинг до кратного 16 байт под uniform-binding). thetaZero/kAngle
// читает compute_bond_angle (2.2b); Morse-kernel их не читает.
struct BondUniforms {
    uint32_t totalCount;
    float thetaZero; // == Bond.cpp:116 (60° в радианах)
    float kAngle;    // == Bond.cpp:121 (50.0)
    uint32_t pad0;   // выравнивание до 16 байт
};
static_assert(sizeof(BondUniforms) == 16, "BondUniforms != 16 байт: разошёлся контракт с physics_bond.wgsl");

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
    // Coulomb layout (6): uniform + pos/charges/off/nbr(read) + forces(rw). Как LJ,
    // но 6 биндингов вместо 7 — charges (binding 2) вместо typeIndices, и НЕТ ljPairs
    // (электростатика не лезет в LJ-таблицу). Per-atom Full-NL gather по ТОМУ ЖЕ NL.
    {
        std::array<wgpu::BindGroupLayoutEntry, 6> e{};
        e[0].binding = 0;
        e[0].visibility = wgpu::ShaderStage::Compute;
        e[0].buffer.type = wgpu::BufferBindingType::Uniform;
        for (uint32_t i = 1; i <= 4; ++i) {
            e[i].binding = i;
            e[i].visibility = wgpu::ShaderStage::Compute;
            e[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        }
        e[5].binding = 5;
        e[5].visibility = wgpu::ShaderStage::Compute;
        e[5].buffer.type = wgpu::BufferBindingType::Storage;
        coulombLayout_ = WGPUContext::instance().createBindGroupLayout(e, "GRP_Coulomb_BGL");
    }
    // Wall layout (3): uniform + positions(read) + forces(rw). Per-atom soft-wall+gravity.
    {
        std::array<wgpu::BindGroupLayoutEntry, 3> e{};
        e[0].binding = 0;
        e[0].visibility = wgpu::ShaderStage::Compute;
        e[0].buffer.type = wgpu::BufferBindingType::Uniform;
        e[1].binding = 1;
        e[1].visibility = wgpu::ShaderStage::Compute;
        e[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        e[2].binding = 2;
        e[2].visibility = wgpu::ShaderStage::Compute;
        e[2].buffer.type = wgpu::BufferBindingType::Storage;
        wallLayout_ = WGPUContext::instance().createBindGroupLayout(e, "GRP_Wall_BGL");
    }
    // Bond Morse layout (6): uniform + positions/bondOffsets/bondNeighbors/bondParams(read)
    // + forces(rw). Per-atom gather по bond-CSR (как LJ по NL, но топология вместо
    // пространственных соседей).
    {
        std::array<wgpu::BindGroupLayoutEntry, 6> e{};
        e[0].binding = 0;
        e[0].visibility = wgpu::ShaderStage::Compute;
        e[0].buffer.type = wgpu::BufferBindingType::Uniform;
        for (uint32_t i = 1; i <= 4; ++i) {
            e[i].binding = i;
            e[i].visibility = wgpu::ShaderStage::Compute;
            e[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        }
        e[5].binding = 5;
        e[5].visibility = wgpu::ShaderStage::Compute;
        e[5].buffer.type = wgpu::BufferBindingType::Storage;
        bondMorseLayout_ = WGPUContext::instance().createBindGroupLayout(e, "GRP_BondMorse_BGL");
    }
    // Bond angle layout (5): uniform + positions/bondOffsets/bondNeighbors(read)
    // + forces(rw). БЕЗ bondParams (binding 4): угловая сила берёт только геометрию
    // + глобальные theta_0/k из uniform, не лезет в Morse-параметры рёбер. Биндинги
    // 0,1,2,3,5 (4 пропущен) — compute_bond_angle статически не ссылается на
    // bondParams, поэтому это ребро не входит в его ресурсный интерфейс.
    {
        std::array<wgpu::BindGroupLayoutEntry, 5> e{};
        e[0].binding = 0;
        e[0].visibility = wgpu::ShaderStage::Compute;
        e[0].buffer.type = wgpu::BufferBindingType::Uniform;
        for (uint32_t i = 1; i <= 3; ++i) {
            e[i].binding = i;
            e[i].visibility = wgpu::ShaderStage::Compute;
            e[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        }
        e[4].binding = 5; // forces (rw) — binding 5 в шейдере (4 — bondParams, не нужен)
        e[4].visibility = wgpu::ShaderStage::Compute;
        e[4].buffer.type = wgpu::BufferBindingType::Storage;
        bondAngleLayout_ = WGPUContext::instance().createBindGroupLayout(e, "GRP_BondAngle_BGL");
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
    wgpu::ShaderModule coulombMod = makeModule(physics_coulombWGSL);
    coulombPipeline_ = makePipeline(*coulombLayout_, coulombMod, "compute_coulomb");
    wgpu::ShaderModule wallMod = makeModule(physics_wallWGSL);
    wallPipeline_ = makePipeline(*wallLayout_, wallMod, "compute_wall");
    wgpu::ShaderModule bondMod = makeModule(physics_bondWGSL);
    bondMorsePipeline_ = makePipeline(*bondMorseLayout_, bondMod, "compute_bond_morse");
    bondAnglePipeline_ = makePipeline(*bondAngleLayout_, bondMod, "compute_bond_angle");
    wgpu::ShaderModule iMod = makeModule(integrate_verletWGSL);
    predictPipeline_ = makePipeline(*intLayout_, iMod, "predict");
    confinePipeline_ = makePipeline(*intLayout_, iMod, "confine");
    zeroPipeline_ = makePipeline(*intLayout_, iMod, "zero_forces");
    correctPipeline_ = makePipeline(*intLayout_, iMod, "correct");
    wgpu::ShaderModule dMod = makeModule(nl_displacementWGSL);
    displacementPipeline_ = makePipeline(*dispLayout_, dMod, "max_displacement");

    ljUniform_ = WGPUContext::instance().createBuffer(sizeof(LJUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                      "GRP_LJU");
    coulombUniform_ = WGPUContext::instance().createBuffer(
        sizeof(CoulombUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "GRP_CoulombU");
    wallUniform_ = WGPUContext::instance().createBuffer(sizeof(WallUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                        "GRP_WallU");
    bondUniform_ = WGPUContext::instance().createBuffer(sizeof(BondUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                        "GRP_BondU");
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
        // stSrc (CopySrc): readbackPotentialEnergy() копирует лейн .w (PE) форс-буфера
        // для прямого наблюдения PE-контракта Coulomb (.w add). CopySrc на rw-буфере
        // безвреден — лишь usage-флаг, доступ kernel'ов не меняет.
        forces_[0] = WGPUContext::instance().createBuffer(cap * 16, stSrc, "GRP_F0");
        forces_[1] = WGPUContext::instance().createBuffer(cap * 16, stSrc, "GRP_F1");
        invMass_ = WGPUContext::instance().createBuffer(cap * 4, st, "GRP_InvMass");
        types_ = WGPUContext::instance().createBuffer(cap * 4, st, "GRP_Types");
        // charges_ зеркалит types_ (тот же cap, f32 = 4 байта). stSrc: CopySrc для
        // readbackCharges() (прямой assert «заряды в VRAM»), CopyDst для writeBuffer.
        charges_ = WGPUContext::instance().createBuffer(cap * 4, stSrc, "GRP_Charges");
        refPos_ = WGPUContext::instance().createBuffer(cap * 16, st, "GRP_RefPos");
        posReadback_ = WGPUContext::instance().createBuffer(cap * 16, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                                            "GRP_PosReadback");
        velReadback_ = WGPUContext::instance().createBuffer(cap * 16, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                                            "GRP_VelReadback");
        atomCapacity_ = cap;
        grew = true;
        // Резидентные pos/vel пересозданы — старый render-bind handle протух. Токен
        // поколения ГЛОБАЛЬНО монотонный (static, общий всем инстансам): после GPU off→on
        // новый инстанс может переиспользовать адрес старого, а per-instance счётчик
        // стартовал бы заново → рендер-кэш bind-group спутал бы новые буфера со старыми
        // (ссылка на освобождённый буфер — серьёзный баг). Глобальный токен исключает
        // коллизию. Тикаем только в этой ветке pos/vel (не на NL-grow — лишний ре-бинд не нужен).
        // Только main-thread (uploadFromCpu/step) — atomic не нужен.
        static uint64_t s_nextRenderBufferGeneration = 1;
        bufferGeneration_ = s_nextRenderBufferGeneration++;
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
    // Bond-CSR (2.2a): bondOffsets_ зеркалит nlOffsets_ (totalCount+1). Растёт здесь,
    // т.к. зависит только от totalCount (известен на каждом ensureCapacity).
    // bondNeighbors_/bondParams_ зависят от числа рёбер (известно лишь в uploadBonds)
    // — здесь резервируем минимум 1 (как nlNeighbors), чтобы первый rebuildBindGroups
    // не получил null-ресурс; реальный рост — в uploadBonds (fail-closed до заливки).
    if (totalCount + 1 > bondOffsetsCapacity_) {
        const size_t cap = kHeadroom(totalCount + 1);
        bondOffsets_ = WGPUContext::instance().createBuffer(cap * 4, stSrc, "GRP_BondOff");
        bondOffsetsCapacity_ = cap;
        grew = true;
    }
    if (bondNeighborsCapacity_ == 0) {
        bondNeighbors_ = WGPUContext::instance().createBuffer(4, stSrc, "GRP_BondNbr");
        bondParams_ = WGPUContext::instance().createBuffer(16, stSrc, "GRP_BondParams");
        bondNeighborsCapacity_ = 1;
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

    // Coulomb bind groups (forces -> forces_[p]) для p=0,1 — тот же ping-pong, что LJ.
    // charges/positions/nlOffsets/nlNeighbors — НЕ ping-pong (одни на оба parity).
    for (int p = 0; p < 2; ++p) {
        std::array<wgpu::BindGroupEntry, 6> b{};
        b[0].binding = 0;
        b[0].buffer = *coulombUniform_;
        b[0].size = sizeof(CoulombUniforms);
        b[1].binding = 1;
        b[1].buffer = *positions_;
        b[1].size = vec4Bytes;
        b[2].binding = 2;
        b[2].buffer = *charges_;
        b[2].size = f32Bytes;
        b[3].binding = 3;
        b[3].buffer = *nlOffsets_;
        b[3].size = nlOffsetsCapacity_ * 4;
        b[4].binding = 4;
        b[4].buffer = *nlNeighbors_;
        b[4].size = nlNeighborsCapacity_ * 4;
        b[5].binding = 5;
        b[5].buffer = *forces_[p];
        b[5].size = vec4Bytes;
        coulombBindGroup_[p] = WGPUContext::instance().createBindGroup(*coulombLayout_, b, "GRP_Coulomb_BG");
    }

    // Wall bind groups (forces -> forces_[p]) для p=0,1 — тот же ping-pong, что LJ.
    for (int p = 0; p < 2; ++p) {
        std::array<wgpu::BindGroupEntry, 3> b{};
        b[0].binding = 0;
        b[0].buffer = *wallUniform_;
        b[0].size = sizeof(WallUniforms);
        b[1].binding = 1;
        b[1].buffer = *positions_;
        b[1].size = vec4Bytes;
        b[2].binding = 2;
        b[2].buffer = *forces_[p];
        b[2].size = vec4Bytes;
        wallBindGroup_[p] = WGPUContext::instance().createBindGroup(*wallLayout_, b, "GRP_Wall_BG");
    }

    // Bond Morse bind groups (forces -> forces_[p]) для p=0,1 — тот же ping-pong.
    // positions/bondOffsets/bondNeighbors/bondParams — НЕ ping-pong (одни на оба
    // parity, как ljPairs/positions у LJ). bondOffsets биндим под totalCount+1 (его
    // активная длина), bondNeighbors/bondParams — под полную ёмкость.
    for (int p = 0; p < 2; ++p) {
        std::array<wgpu::BindGroupEntry, 6> b{};
        b[0].binding = 0;
        b[0].buffer = *bondUniform_;
        b[0].size = sizeof(BondUniforms);
        b[1].binding = 1;
        b[1].buffer = *positions_;
        b[1].size = vec4Bytes;
        b[2].binding = 2;
        b[2].buffer = *bondOffsets_;
        b[2].size = bondOffsetsCapacity_ * 4;
        b[3].binding = 3;
        b[3].buffer = *bondNeighbors_;
        b[3].size = bondNeighborsCapacity_ * 4;
        b[4].binding = 4;
        b[4].buffer = *bondParams_;
        b[4].size = bondNeighborsCapacity_ * 16;
        b[5].binding = 5;
        b[5].buffer = *forces_[p];
        b[5].size = vec4Bytes;
        bondMorseBindGroup_[p] = WGPUContext::instance().createBindGroup(*bondMorseLayout_, b, "GRP_BondMorse_BG");
    }

    // Bond angle bind groups (forces -> forces_[p]) для p=0,1 — тот же ping-pong.
    // 5 биндингов (без bondParams): uniform + positions + bondOffsets + bondNeighbors
    // + forces. Биндинги 0,1,2,3,5 (4 пропущен) — совпадают с bondAngleLayout_.
    // positions/bondOffsets/bondNeighbors — НЕ ping-pong (одни на оба parity).
    for (int p = 0; p < 2; ++p) {
        std::array<wgpu::BindGroupEntry, 5> b{};
        b[0].binding = 0;
        b[0].buffer = *bondUniform_;
        b[0].size = sizeof(BondUniforms);
        b[1].binding = 1;
        b[1].buffer = *positions_;
        b[1].size = vec4Bytes;
        b[2].binding = 2;
        b[2].buffer = *bondOffsets_;
        b[2].size = bondOffsetsCapacity_ * 4;
        b[3].binding = 3;
        b[3].buffer = *bondNeighbors_;
        b[3].size = bondNeighborsCapacity_ * 4;
        b[4].binding = 5; // forces (rw) на binding 5 (4 — bondParams, угол не читает)
        b[4].buffer = *forces_[p];
        b[4].size = vec4Bytes;
        bondAngleBindGroup_[p] = WGPUContext::instance().createBindGroup(*bondAngleLayout_, b, "GRP_BondAngle_BG");
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
                                       float worldSizeX, float worldSizeY, float worldSizeZ, float gravityX, float gravityY,
                                       float gravityZ, bool ljEnabled, bool coulombEnabled) {
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
    gravity_[0] = gravityX;
    gravity_[1] = gravityY;
    gravity_[2] = gravityZ;
    ljEnabled_ = ljEnabled; // step() пропустит compute_lj если false (паритет с CPU isLJEnabled)
    coulombEnabled_ = coulombEnabled; // step() пропустит compute_coulomb если false (паритет с CPU isCoulombEnabled)
    parity_ = 0;

    std::vector<float> pos(n * 4), vel(n * 4), im(n), ch(n);
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
        ch[i] = atoms.charge(i); // заряд как есть из CPU (без арифметики) → бит-идентичен гарду
    }

    auto q = WGPUContext::instance().queue();
    q->writeBuffer(*positions_, 0, pos.data(), pos.size() * 4);
    q->writeBuffer(*velocities_, 0, vel.data(), vel.size() * 4);
    q->writeBuffer(*invMass_, 0, im.data(), im.size() * 4);
    q->writeBuffer(*types_, 0, ty.data(), ty.size() * 4);
    q->writeBuffer(*charges_, 0, ch.data(), ch.size() * 4);
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
    // Coulomb uniform: тот же cutoffSqr/epsilon, что LJ (общий NL + Consts::Epsilon);
    // kCoulomb == CoulombForceField.h:14 (физическая константа в uniform ради читаемого
    // контракта, единообразно с тем как wall держит k/border, а bond — theta_0/k_angle).
    CoulombUniforms cu{cutoffSqr_, 1e-6f, mobileCount_, CoulombForceField::kCoulombEvAngstrom};
    q->writeBuffer(*coulombUniform_, 0, &cu, sizeof(cu));
    DispUniforms du{mobileCount_};
    q->writeBuffer(*dispUniform_, 0, &du, sizeof(du));

    // Wall uniform: worldMax (== confine max), gravity (постоянная СИЛА), k/border
    // (CPU-инварианты WallForceField.cpp:28-29). Полная перезаливка несёт текущую
    // gravity, поэтому рантайм-смена gravity подхватывается ближайшим re-upload'ом
    // (Simulation::setGravity бампит cpuSceneVersion → updateStateGpu перезаливает).
    WallUniforms wu{};
    wu.worldMaxX = worldMax_[0];
    wu.worldMaxY = worldMax_[1];
    wu.worldMaxZ = worldMax_[2];
    wu.gravityX = gravity_[0];
    wu.gravityY = gravity_[1];
    wu.gravityZ = gravity_[2];
    wu.k = 500.0f;     // == WallForceField.cpp:28
    wu.border = 2.0f;  // == WallForceField.cpp:29
    wu.mobileCount = mobileCount_;
    q->writeBuffer(*wallUniform_, 0, &wu, sizeof(wu));

    // Bond uniform: totalCount (гард kernel'а i >= totalCount при gTotal-диспатче).
    // thetaZero/kAngle — глобальные хардкод-инварианты угловой модели (== Bond.cpp:116,121),
    // читает compute_bond_angle (2.2b); Morse-kernel их не читает. 60°·π/180 = 1.04719755.
    BondUniforms bu{};
    bu.totalCount = totalCount_;
    bu.thetaZero = 60.0f / 180.0f * 3.14159265358979323846f; // == Bond.cpp:116 (theta_0)
    bu.kAngle = 50.0f;                                        // == Bond.cpp:121 (k)
    q->writeBuffer(*bondUniform_, 0, &bu, sizeof(bu));
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

void GpuResidentPhysics::uploadBonds(const Bond::List& bonds, const AtomStorage& atoms) {
    ensureInitialized();
    const size_t n = static_cast<size_t>(totalCount_);

    // CSR строим в CPU-векторах, зеркаля порядок BondForceField (degree → prefix
    // sum → fill двусторонним обходом). totalCount_ уже выставлен uploadFromCpu
    // (вызывается до uploadBonds в uploadSceneToGpu).
    std::vector<uint32_t> bondOffsets(n + 1, 0u);

    // 1) Степени per-atom: каждый bond инкрементит оба конца (== degreeScratch_,
    //    BondForceField.cpp:109-115). Out-of-range конец пропускаем (как CPU-гард
    //    bond.aIndex<n && bond.bIndex<n) — иначе degree/fill разъедутся.
    for (const Bond& bond : bonds) {
        if (bond.aIndex < n && bond.bIndex < n) {
            ++bondOffsets[bond.aIndex];
            ++bondOffsets[bond.bIndex];
        }
    }

    // 2) Prefix sum → offsets (CSR): bondOffsets[i] = начало окна атома i,
    //    bondOffsets[n] = суммарное число directed-рёбер (= 2*validBondCount).
    uint32_t running = 0u;
    for (size_t i = 0; i < n; ++i) {
        const uint32_t deg = bondOffsets[i];
        bondOffsets[i] = running;
        running += deg;
    }
    bondOffsets[n] = running;
    const uint32_t directedEdges = running; // 2 * число валидных связей

    // 3) Fill neighbors + params двусторонним обходом в порядке списка bonds (==
    //    BondForceField.cpp:129-134: для bond (a,b) сосед b кладётся атому a, сосед
    //    a — атому b). Бегущий курсор per-atom сохраняет ТОТ ЖЕ порядок соседей,
    //    что CPU emplace_back. Params каждого ребра — из bond.params (Bond.h:40),
    //    зафиксированы при создании связи; kernel НЕ лезет в type-таблицу.
    std::vector<uint32_t> bondNeighbors(std::max<uint32_t>(directedEdges, 1u), 0u);
    std::vector<float> bondParams(std::max<uint32_t>(directedEdges, 1u) * 4u, 0.0f);
    std::vector<uint32_t> cursor(n, 0u);
    for (size_t i = 0; i < n; ++i) {
        cursor[i] = bondOffsets[i];
    }
    auto writeEdge = [&](uint32_t from, uint32_t to, const BondParams& p) {
        const uint32_t slot = cursor[from]++;
        bondNeighbors[slot] = to;
        bondParams[slot * 4 + 0] = p.r0;
        bondParams[slot * 4 + 1] = p.De;
        bondParams[slot * 4 + 2] = p.a;
        bondParams[slot * 4 + 3] = 0.0f;
    };
    for (const Bond& bond : bonds) {
        if (bond.aIndex < n && bond.bIndex < n) {
            writeEdge(static_cast<uint32_t>(bond.aIndex), static_cast<uint32_t>(bond.bIndex), bond.params);
            writeEdge(static_cast<uint32_t>(bond.bIndex), static_cast<uint32_t>(bond.aIndex), bond.params);
        }
    }

    // 4) Рост резидентных буферов ДО writeBuffer (fail-closed, как NL overflow в
    //    rebuildNeighborListOnGpu): bondOffsets_ уже >= totalCount+1 после
    //    ensureCapacity в uploadFromCpu; bondNeighbors_/bondParams_ растим под
    //    directedEdges, если прежней ёмкости мало (re-upload с бо́льшим числом связей).
    if (static_cast<size_t>(directedEdges) > bondNeighborsCapacity_) {
        const size_t cap = kHeadroom(directedEdges);
        const wgpu::BufferUsage stSrc =
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
        bondNeighbors_ = WGPUContext::instance().createBuffer(cap * 4, stSrc, "GRP_BondNbr");
        bondParams_ = WGPUContext::instance().createBuffer(cap * 16, stSrc, "GRP_BondParams");
        bondNeighborsCapacity_ = cap;
        rebuildBindGroups(); // буфера пересозданы — перепривязать bond-группы (и все прочие)
    }

    bondNeighborCount_ = directedEdges;

    auto q = WGPUContext::instance().queue();
    q->writeBuffer(*bondOffsets_, 0, bondOffsets.data(), bondOffsets.size() * 4);
    // bondNeighbors/bondParams всегда непусты (min 1 элемент при отсутствии связей —
    // offsets все равны 0, gather по пустому окну даёт 0). Заливаем активную длину.
    q->writeBuffer(*bondNeighbors_, 0, bondNeighbors.data(), bondNeighbors.size() * 4);
    q->writeBuffer(*bondParams_, 0, bondParams.data(), bondParams.size() * 4);
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

// Блокирующий readback float-буфера (bench/диагностика, как readU32Blocking).
// strideFloats — шаг между нужными элементами (1 для плотного array<f32>, 4 для
// лейна vec4<f32> при чтении одной компоненты); offsetFloats — смещение первого
// нужного элемента (3 для .w в vec4). Копирует count полных элементов с GPU, затем
// выбирает count/strideFloats значений со смещением offsetFloats.
std::vector<float> readF32Blocking(const wgpu::raii::Buffer& src, uint32_t totalFloats, uint32_t strideFloats,
                                   uint32_t offsetFloats) {
    if (totalFloats == 0u || strideFloats == 0u) {
        return {};
    }
    const uint64_t bytes = static_cast<uint64_t>(totalFloats) * 4;
    wgpu::Device dev = *WGPUContext::instance().device();
    wgpu::Buffer rb = WGPUContext::instance().createBuffer(bytes, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                                           "GRP_F32Readback");

    wgpu::CommandEncoder enc = dev.createCommandEncoder({});
    enc.copyBufferToBuffer(*src, 0, rb, 0, bytes);
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
        throw std::runtime_error("GpuResidentPhysics: f32 readback map failed");
    }
    const float* data = static_cast<const float*>(rb.getConstMappedRange(0, bytes));
    std::vector<float> out;
    out.reserve(totalFloats / strideFloats);
    for (uint32_t i = offsetFloats; i < totalFloats; i += strideFloats) {
        out.push_back(data[i]);
    }
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

std::vector<uint32_t> GpuResidentPhysics::readbackBondOffsets() const {
    return readU32Blocking(bondOffsets_, totalCount_ + 1u, 0u);
}

std::vector<uint32_t> GpuResidentPhysics::readbackBondNeighbors(uint32_t total) const {
    if (total == 0u) {
        return {};
    }
    return readU32Blocking(bondNeighbors_, total, 0u);
}

std::vector<float> GpuResidentPhysics::readbackCharges() const {
    // charges_ — плотный array<f32> длины totalCount (stride 1, offset 0).
    return readF32Blocking(charges_, totalCount_, 1u, 0u);
}

std::vector<float> GpuResidentPhysics::readbackPotentialEnergy() const {
    // Лейн .w текущего force-буфера (parity_): vec4<f32> на атом → totalCount*4 float,
    // .w — каждый 4-й со смещением 3. parity_ указывает на буфер, записанный последним
    // step()'ом (forces_[out], out стал parity_ в конце step) — несёт PE последнего шага.
    return readF32Blocking(forces_[parity_], totalCount_ * 4u, 4u, 3u);
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

    // zero + wall + LJ пишут out = forces_[1-p]; correct читает out + in.
    // intBindGroup_[out] имеет forces=forces_[out], prevForces=forces_[p]=in.
    pass.setBindGroup(0, *intBindGroup_[out], 0, nullptr);
    pass.setPipeline(*zeroPipeline_);
    pass.dispatchWorkgroups(gTotal, 1, 1);

    // wall+gravity (mobile) в forces_[out], МЕЖДУ zero и LJ — зеркалит CPU-порядок
    // wallForceField_.compute ПЕРЕД computePairInteractions (ForceField.cpp:136-137).
    // Аккумулятивно прибавляет к обнулённой силе; correct видит wall+gravity+LJ.
    // Memory-ordering между zero→wall→LJ — та же гарантия storage-барьеров WebGPU
    // в одном compute-pass, на которую уже полагается zero→LJ→correct.
    pass.setBindGroup(0, *wallBindGroup_[out], 0, nullptr);
    pass.setPipeline(*wallPipeline_);
    pass.dispatchWorkgroups(gMobile, 1, 1);

    // LJ (mobile) — ПРОПУСКАЕМ диспатч, если LJ выключен (паритет с CPU
    // ForceField::compute, который чекает isLJEnabled, ForceField.cpp:147-150).
    // Раньше диспатч был безусловным → setLJEnabled(false) молча игнорился на GPU.
    if (ljEnabled_) {
        pass.setBindGroup(0, *ljBindGroup_[out], 0, nullptr);
        pass.setPipeline(*ljPipeline_);
        pass.dispatchWorkgroups(gMobile, 1, 1);
    }

    // Coulomb (mobile) ПОСЛЕ LJ, ПЕРЕД bonds — зеркалит CPU-порядок (LJ и Coulomb в
    // одном pair-loop computePairInteractions ПЕРЕД bonds, ForceField.cpp:57-82,136-138).
    // gMobile (как LJ): Coulomb — mobile-mobile pair-сила с fixed-skip (CPU pair-loop
    // не обрабатывает fixed-центр и пропускает fixed-соседа, ForceField.cpp:59-62,120-123).
    // Charge-gated: kernel сам делает return при chargeA==0, поэтому на нейтральной
    // сцене добавляет ровно 0 (LJ-only паритет цел). ПРОПУСКАЕМ диспатч, если Coulomb
    // выключен (паритет с CPU isCoulombEnabled). Аккумулятивно прибавляет к forces_[out]
    // .xyz И к .w (PE): .w после zero->wall->LJ->coulomb несёт LJ_PE + Coulomb_PE.
    // Memory-ordering между LJ→coulomb→bonds — та же гарантия storage-барьеров WebGPU
    // в одном compute-pass, на которую уже полагается zero→wall→LJ→bonds→correct.
    if (coulombEnabled_) {
        pass.setBindGroup(0, *coulombBindGroup_[out], 0, nullptr);
        pass.setPipeline(*coulombPipeline_);
        pass.dispatchWorkgroups(gMobile, 1, 1);
    }

    // Morse-силы связей (gTotal) ПОСЛЕ LJ, ПЕРЕД correct — зеркалит CPU-порядок
    // wall→LJ→bonds (ForceField.cpp:136-138; bonds последними). gTotal (а не gMobile):
    // Bond::forceBond пишет силу по индексам концов без mobile-фильтра (Bond.cpp:51-57);
    // запись в fixed-слот безвредна (correct по gMobile её игнорит). Аккумулятивно
    // прибавляет к forces_[out].xyz; .w (PE) не трогает. При пустой adjacency
    // (сцена без связей) gather по пустым окнам даёт ровно 0 → LJ-only паритет цел.
    pass.setBindGroup(0, *bondMorseBindGroup_[out], 0, nullptr);
    pass.setPipeline(*bondMorsePipeline_);
    pass.dispatchWorkgroups(gTotal, 1, 1);

    // Угловые силы связей (gTotal) ПОСЛЕ Morse, ПЕРЕД correct — зеркалит CPU-порядок
    // (BondForceField::compute: цикл forceBond ПЕРЕД applyAngleForces, BondForceField.cpp:45-49).
    // Порядок morse→angle математически не важен (оба только прибавляют к forces[out].xyz,
    // ни один не читает forces — читают positions+bond-CSR), но зеркалит CPU для очевидности.
    // gTotal как Morse (Bond::angleForce пишет по индексам без mobile-фильтра, Bond.cpp:133-143;
    // запись в fixed-слот безвредна). При пустой/degree-1 adjacency прибавляет ровно 0
    // (нет пар рёбер у центра, нет двух-хоповых троек) → LJ-only и Morse-only паритет цел.
    pass.setBindGroup(0, *bondAngleBindGroup_[out], 0, nullptr);
    pass.setPipeline(*bondAnglePipeline_);
    pass.dispatchWorkgroups(gTotal, 1, 1);

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
    ++downloadCount_; // B: телеметрия zero-copy — считаем РЕАЛЬНЫЕ GPU->CPU скачивания
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
