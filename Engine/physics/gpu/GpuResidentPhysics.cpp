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
GpuResidentPhysics::~GpuResidentPhysics() = default;

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
        nlNeighbors_ = WGPUContext::instance().createBuffer(cap * 4, st, "GRP_Nbr");
        nlNeighborsCapacity_ = cap;
        grew = true;
    }
    if (totalCount + 1 > nlOffsetsCapacity_) {
        const size_t cap = kHeadroom(totalCount + 1);
        nlOffsets_ = WGPUContext::instance().createBuffer(cap * 4, st, "GRP_Off");
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
}

void GpuResidentPhysics::downloadToCpu(AtomStorage& atoms, bool withVelocities) {
    const size_t n = totalCount_;
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

float GpuResidentPhysics::maxDisplacementSqr() {
    wgpu::Device dev = *WGPUContext::instance().device();
    // reset flag (CPU write 0), затем max_displacement kernel, затем readback.
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

    struct MapCtx {
        bool done;
        bool ok;
    } ctx{false, true};
    auto cb = [](WGPUMapAsyncStatus s, WGPUStringView, void* u1, void*) {
        auto* c = static_cast<MapCtx*>(u1);
        c->done = true;
        c->ok = (s == WGPUMapAsyncStatus_Success);
    };
    wgpu::BufferMapCallbackInfo ci{};
    ci.mode = wgpu::CallbackMode::AllowSpontaneous;
    ci.callback = cb;
    ci.userdata1 = &ctx;
    dispReadback_->mapAsync(wgpu::MapMode::Read, 0, sizeof(uint32_t), ci);
    while (!ctx.done) {
        dev.poll(true, nullptr);
    }
    if (!ctx.ok) {
        throw std::runtime_error("GpuResidentPhysics: displacement map failed");
    }
    const uint32_t* bits = static_cast<const uint32_t*>(dispReadback_->getConstMappedRange(0, sizeof(uint32_t)));
    uint32_t maxBits = *bits;
    dispReadback_->unmap();

    float result;
    std::memcpy(&result, &maxBits, sizeof(result));
    return result;
}
