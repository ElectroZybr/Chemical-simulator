// Доказательство win резидентного GPU-шага: predict + confine + zero + LJ +
// correct целиком на GPU, БЕЗ per-step readback. Это число решает, оправдан ли
// resident integrator (микробенч BM_GpuLJReorder показал, что kernel ~83 us и
// не bottleneck — весь выигрыш в устранении ~3400 us readback).
//
// Методология: K резидентных шагов (5 dispatch каждый) в одном submit + один
// poll. Per-step = wall/K. Буфера резидентны, между шагами ничего не качается.
//
// Сравнивать с CPU SimulationFixture/FullStepWithNeighborList (RESULTS.md) и
// CPU pair 925 us. Это steady-state без NL rebuild / render sync — верхняя
// граница выигрыша; rebuild/render cadence моделируются отдельно при инженерной
// интеграции.

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <benchmark/benchmark.h>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Benchmarks/fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Rendering/WGPUContext.h"

#include "generated/shaders/integrate_verlet.wgsl.h"
#include "generated/shaders/physics_lj.wgsl.h"

namespace {

constexpr int kStepsPerSubmit = 20;

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

class FullStepHarness {
public:
    void build(int atomCount) {
        benchmarkDevice();
        wgpu::Device dev = *WGPUContext::instance().device();

        // --- Сцена: кубическая решётка, Full NL, LJ-only ---
        const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount))) + 1;
        const float spacing = 3.0f;
        const float worldSize = side * spacing + 20.0f;
        Simulation sim;
        sim.createWorld(Vec3f{worldSize, worldSize, worldSize});
        sim.setSizeBox(sim.world().getWorldSize(), 6);
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.neighborList().setMode(NeighborListMode::Full);
        int placed = 0;
        for (int z = 0; z < side && placed < atomCount; ++z) {
            for (int y = 0; y < side && placed < atomCount; ++y) {
                for (int x = 0; x < side && placed < atomCount; ++x) {
                    sim.appendAtomFast(Vec3f{10.0f + x * spacing, 10.0f + y * spacing, 10.0f + z * spacing},
                                       Vec3f{0.0f, 0.0f, 0.0f}, AtomData::Type::H, false);
                    ++placed;
                }
            }
        }
        sim.finalizeAtomBatch();
        sim.neighborList().build(sim.atoms(), sim.world());

        const AtomStorage& atoms = sim.atoms();
        mobileCount_ = static_cast<uint32_t>(atoms.mobileCount());
        totalCount_ = static_cast<uint32_t>(atoms.size());
        const auto& offsets = sim.neighborList().offsets();
        const auto& neighbors = sim.neighborList().neighbors();

        const wgpu::BufferUsage st = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        auto mk = [&](uint64_t bytes, const char* n) { return WGPUContext::instance().createBuffer(bytes, st, n); };

        positions_ = mk(totalCount_ * 16, "FS_Pos");
        velocities_ = mk(totalCount_ * 16, "FS_Vel");
        forces_ = mk(totalCount_ * 16, "FS_Forces");
        prevForces_ = mk(totalCount_ * 16, "FS_PrevForces");
        invMass_ = mk(totalCount_ * 4, "FS_InvMass");
        types_ = mk(totalCount_ * 4, "FS_Types");
        nlOffsets_ = mk(offsets.size() * 4, "FS_Off");
        nlNeighbors_ = mk(std::max<size_t>(neighbors.size(), 1) * 4, "FS_Nbr");
        ljUniform_ = WGPUContext::instance().createBuffer(sizeof(LJUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
                                                          "FS_LJU");
        intUniform_ = WGPUContext::instance().createBuffer(sizeof(IntegratorUniforms),
                                                           wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "FS_IntU");

        // upload scene
        std::vector<float> pos(totalCount_ * 4), vel(totalCount_ * 4, 0.0f), im(totalCount_);
        std::vector<uint32_t> ty(totalCount_);
        for (uint32_t i = 0; i < totalCount_; ++i) {
            pos[i * 4 + 0] = atoms.posX(i);
            pos[i * 4 + 1] = atoms.posY(i);
            pos[i * 4 + 2] = atoms.posZ(i);
            pos[i * 4 + 3] = 0.0f;
            im[i] = atoms.invMass(i);
            ty[i] = static_cast<uint32_t>(atoms.type(i));
        }
        constexpr size_t kTC = static_cast<size_t>(AtomData::Type::COUNT);
        std::vector<float> ljPacked(kTC * kTC * 2);
        LJForceField lj;
        for (size_t i = 0; i < kTC; ++i) {
            const auto& row = lj.pairRow(static_cast<AtomData::Type>(i));
            for (size_t j = 0; j < kTC; ++j) {
                ljPacked[(i * kTC + j) * 2 + 0] = row[j].potentialC6;
                ljPacked[(i * kTC + j) * 2 + 1] = row[j].potentialC12;
            }
        }
        ljPairs_ = mk(ljPacked.size() * 4, "FS_LJPairs");

        auto q = WGPUContext::instance().queue();
        q->writeBuffer(*positions_, 0, pos.data(), pos.size() * 4);
        q->writeBuffer(*velocities_, 0, vel.data(), vel.size() * 4);
        q->writeBuffer(*invMass_, 0, im.data(), im.size() * 4);
        q->writeBuffer(*types_, 0, ty.data(), ty.size() * 4);
        q->writeBuffer(*nlOffsets_, 0, offsets.data(), offsets.size() * 4);
        if (!neighbors.empty()) {
            q->writeBuffer(*nlNeighbors_, 0, neighbors.data(), neighbors.size() * 4);
        }
        q->writeBuffer(*ljPairs_, 0, ljPacked.data(), ljPacked.size() * 4);

        LJUniforms lju{};
        lju.cutoffSqr = sim.neighborList().cutoff() * sim.neighborList().cutoff();
        lju.epsilon = 1e-6f;
        lju.mobileCount = mobileCount_;
        lju.typeCount = static_cast<uint32_t>(kTC);
        q->writeBuffer(*ljUniform_, 0, &lju, sizeof(lju));

        IntegratorUniforms iu{};
        iu.dt = 0.01f;
        iu.accelDamping = 0.9f;
        iu.worldMaxX = worldSize - 1.0f;
        iu.worldMaxY = worldSize - 1.0f;
        iu.worldMaxZ = worldSize - 1.0f;
        iu.restitution = 0.8f;
        iu.mobileCount = mobileCount_;
        iu.totalCount = totalCount_;
        q->writeBuffer(*intUniform_, 0, &iu, sizeof(iu));

        buildPipelines(offsets.size(), neighbors.size(), ljPacked.size());
    }

    void dispatchSteps(int steps) {
        wgpu::Device dev = *WGPUContext::instance().device();
        wgpu::CommandEncoder enc = dev.createCommandEncoder({});
        wgpu::ComputePassEncoder pass = enc.beginComputePass({});
        const uint32_t gMobile = (mobileCount_ + 63u) / 64u;
        const uint32_t gTotal = (totalCount_ + 63u) / 64u;
        for (int s = 0; s < steps; ++s) {
            pass.setBindGroup(0, *intBG_, 0, nullptr);
            pass.setPipeline(*predict_);
            pass.dispatchWorkgroups(gMobile, 1, 1);
            pass.setPipeline(*confine_);
            pass.dispatchWorkgroups(gMobile, 1, 1);
            pass.setPipeline(*zeroForces_);
            pass.dispatchWorkgroups(gTotal, 1, 1);
            pass.setBindGroup(0, *ljBG_, 0, nullptr);
            pass.setPipeline(*lj_);
            pass.dispatchWorkgroups(gMobile, 1, 1);
            pass.setBindGroup(0, *intBG_, 0, nullptr);
            pass.setPipeline(*correct_);
            pass.dispatchWorkgroups(gMobile, 1, 1);
        }
        pass.end();
        wgpu::CommandBuffer cmd = enc.finish({});
        WGPUContext::instance().queue()->submit(1, &cmd);
        dev.poll(true, nullptr);
    }

private:
    void buildPipelines(size_t offN, size_t nbrN, size_t ljN) {
        wgpu::Device dev = *WGPUContext::instance().device();

        // LJ bind layout (7): uniform + pos/types/off/nbr/ljPairs(read) + forces(rw)
        std::array<wgpu::BindGroupLayoutEntry, 7> ljE{};
        ljE[0].binding = 0;
        ljE[0].visibility = wgpu::ShaderStage::Compute;
        ljE[0].buffer.type = wgpu::BufferBindingType::Uniform;
        for (uint32_t i = 1; i <= 5; ++i) {
            ljE[i].binding = i;
            ljE[i].visibility = wgpu::ShaderStage::Compute;
            ljE[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        }
        ljE[6].binding = 6;
        ljE[6].visibility = wgpu::ShaderStage::Compute;
        ljE[6].buffer.type = wgpu::BufferBindingType::Storage;
        ljBGL_ = WGPUContext::instance().createBindGroupLayout(ljE, "FS_LJ_BGL");

        // Integrator bind layout (6): uniform + pos/vel/forces(rw) + prevForces/invMass(read)
        std::array<wgpu::BindGroupLayoutEntry, 6> iE{};
        iE[0].binding = 0;
        iE[0].visibility = wgpu::ShaderStage::Compute;
        iE[0].buffer.type = wgpu::BufferBindingType::Uniform;
        iE[1].binding = 1;
        iE[1].visibility = wgpu::ShaderStage::Compute;
        iE[1].buffer.type = wgpu::BufferBindingType::Storage;
        iE[2].binding = 2;
        iE[2].visibility = wgpu::ShaderStage::Compute;
        iE[2].buffer.type = wgpu::BufferBindingType::Storage;
        iE[3].binding = 3;
        iE[3].visibility = wgpu::ShaderStage::Compute;
        iE[3].buffer.type = wgpu::BufferBindingType::Storage;
        iE[4].binding = 4;
        iE[4].visibility = wgpu::ShaderStage::Compute;
        iE[4].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        iE[5].binding = 5;
        iE[5].visibility = wgpu::ShaderStage::Compute;
        iE[5].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        intBGL_ = WGPUContext::instance().createBindGroupLayout(iE, "FS_Int_BGL");

        wgpu::ShaderModule ljMod = makeModule(physics_ljWGSL);
        lj_ = makePipeline(*ljBGL_, ljMod, "compute_lj");
        wgpu::ShaderModule iMod = makeModule(integrate_verletWGSL);
        predict_ = makePipeline(*intBGL_, iMod, "predict");
        confine_ = makePipeline(*intBGL_, iMod, "confine");
        zeroForces_ = makePipeline(*intBGL_, iMod, "zero_forces");
        correct_ = makePipeline(*intBGL_, iMod, "correct");

        // LJ bind group
        std::array<wgpu::BindGroupEntry, 7> lb{};
        lb[0].binding = 0;
        lb[0].buffer = *ljUniform_;
        lb[0].size = sizeof(LJUniforms);
        lb[1].binding = 1;
        lb[1].buffer = *positions_;
        lb[1].size = totalCount_ * 16;
        lb[2].binding = 2;
        lb[2].buffer = *types_;
        lb[2].size = totalCount_ * 4;
        lb[3].binding = 3;
        lb[3].buffer = *nlOffsets_;
        lb[3].size = offN * 4;
        lb[4].binding = 4;
        lb[4].buffer = *nlNeighbors_;
        lb[4].size = std::max<size_t>(nbrN, 1) * 4;
        lb[5].binding = 5;
        lb[5].buffer = *ljPairs_;
        lb[5].size = ljN * 4;
        lb[6].binding = 6;
        lb[6].buffer = *forces_;
        lb[6].size = totalCount_ * 16;
        ljBG_ = WGPUContext::instance().createBindGroup(*ljBGL_, lb, "FS_LJ_BG");

        // Integrator bind group
        std::array<wgpu::BindGroupEntry, 6> ib{};
        ib[0].binding = 0;
        ib[0].buffer = *intUniform_;
        ib[0].size = sizeof(IntegratorUniforms);
        ib[1].binding = 1;
        ib[1].buffer = *positions_;
        ib[1].size = totalCount_ * 16;
        ib[2].binding = 2;
        ib[2].buffer = *velocities_;
        ib[2].size = totalCount_ * 16;
        ib[3].binding = 3;
        ib[3].buffer = *forces_;
        ib[3].size = totalCount_ * 16;
        ib[4].binding = 4;
        ib[4].buffer = *prevForces_;
        ib[4].size = totalCount_ * 16;
        ib[5].binding = 5;
        ib[5].buffer = *invMass_;
        ib[5].size = totalCount_ * 4;
        intBG_ = WGPUContext::instance().createBindGroup(*intBGL_, ib, "FS_Int_BG");
    }

    uint32_t mobileCount_ = 0, totalCount_ = 0;
    wgpu::raii::Buffer positions_, velocities_, forces_, prevForces_, invMass_, types_, nlOffsets_, nlNeighbors_, ljPairs_;
    wgpu::raii::Buffer ljUniform_, intUniform_;
    wgpu::raii::BindGroupLayout ljBGL_, intBGL_;
    wgpu::raii::BindGroup ljBG_, intBG_;
    wgpu::raii::ComputePipeline lj_, predict_, confine_, zeroForces_, correct_;
};

void runFullStep(benchmark::State& state) {
    const int atomCount = static_cast<int>(state.range(0));
    FullStepHarness h;
    h.build(atomCount);
    h.dispatchSteps(2); // warmup
    for (auto _ : state) {
        h.dispatchSteps(kStepsPerSubmit);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kStepsPerSubmit) * atomCount);
    state.counters["steps_per_iter"] = kStepsPerSubmit;
}

} // namespace

// @bench_meta {"id":"GpuFullStep/Resident","ru":"Резидентный GPU full step (без readback)","group":"Симуляция/GPU"}
void BM_GpuFullStep_Resident(benchmark::State& state) { runFullStep(state); }

BENCHMARK(BM_GpuFullStep_Resident)->Arg(15625)->Arg(103823)->Unit(benchmark::kMicrosecond);
