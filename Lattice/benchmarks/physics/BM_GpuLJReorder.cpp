// De-risking microbench: измеряет влияние порядка атомов на время GPU LJ kernel.
//
// Цель: доказать/опровергнуть, что spatial reorder
// (Morton) даёт заявленное ускорение kernel'а за счёт cache-locality gather'а
// positions[j]. Это gate для всего GPU roadmap — если reorder не уводит kernel
// заметно ниже CPU pair baseline (~0.925 ms на 103k), следующий этап не оставит
// бюджета на integrator/submit/displacement.
//
// Методология:
//   - НЕ wall-clock вокруг blocking readback (он ~1ms swamp'ит kernel delta).
//   - Batch из kDispatchesPerSubmit dispatch'ей в одном submit + один poll →
//     fixed submit/poll overhead амортизируется, остаётся ~чистое kernel-время.
//   - Три порядка: Identity (как построено), Random (worst case), Morton (best).
//   - Reuse shipped kernel physics_lj.wgsl без изменений, engine не трогаем.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Rendering/backend/WGPUContext.h"

#include "generated/shaders/physics_lj.wgsl.h"
using namespace Lattice;

namespace {

constexpr int kDispatchesPerSubmit = 50;

enum class Ordering { Identity, Random, Morton };

// Morton (Z-order) код из 10-битных на ось cell-координат.
uint32_t mortonExpandBits(uint32_t v) {
    v &= 0x3ff;
    v = (v | (v << 16)) & 0x030000FF;
    v = (v | (v << 8)) & 0x0300F00F;
    v = (v | (v << 4)) & 0x030C30C3;
    v = (v | (v << 2)) & 0x09249249;
    return v;
}
uint32_t mortonCode(uint32_t x, uint32_t y, uint32_t z) {
    return (mortonExpandBits(x) << 2) | (mortonExpandBits(y) << 1) | mortonExpandBits(z);
}

struct SceneData {
    std::vector<float> positions; // vec4 (x,y,z,0) packed
    std::vector<uint32_t> types;
    std::vector<uint32_t> nlOffsets;
    std::vector<uint32_t> nlNeighbors;
    uint32_t mobileCount = 0;
    uint32_t totalCount = 0;
    float cutoff = 5.0f;
};

// Строит базовую кубическую решётку, применяет перестановку по ordering,
// затем создаёт Simulation в этом порядке и строит Full NL. NL получается
// уже в новых индексах (build идёт по факт. порядку атомов в storage), так
// что ручной remap не нужен — это самый надёжный способ.
SceneData buildScene(int atomCount, Ordering ordering, float spacing, float cellSize) {
    const int side = static_cast<int>(std::cbrt(static_cast<double>(atomCount))) + 1;

    // Базовые позиции в порядке вложенных циклов z,y,x.
    struct P {
        float x, y, z;
        uint32_t cx, cy, cz;
    };
    std::vector<P> base;
    base.reserve(atomCount);
    for (int z = 0; z < side && static_cast<int>(base.size()) < atomCount; ++z) {
        for (int y = 0; y < side && static_cast<int>(base.size()) < atomCount; ++y) {
            for (int x = 0; x < side && static_cast<int>(base.size()) < atomCount; ++x) {
                const float fx = 10.0f + x * spacing;
                const float fy = 10.0f + y * spacing;
                const float fz = 10.0f + z * spacing;
                base.push_back({fx, fy, fz, static_cast<uint32_t>(fx / cellSize), static_cast<uint32_t>(fy / cellSize),
                                static_cast<uint32_t>(fz / cellSize)});
            }
        }
    }

    // Перестановка индексов: в каком порядке атомы будут добавлены в Simulation.
    std::vector<uint32_t> order(base.size());
    std::iota(order.begin(), order.end(), 0u);
    if (ordering == Ordering::Random) {
        std::mt19937 rng(20260528);
        std::shuffle(order.begin(), order.end(), rng);
    }
    else if (ordering == Ordering::Morton) {
        std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
            return mortonCode(base[a].cx, base[a].cy, base[a].cz) < mortonCode(base[b].cx, base[b].cy, base[b].cz);
        });
    }
    // Identity — порядок уже z,y,x (как заполняет реальная решётка).

    Simulation sim;
    sim.createWorld(glm::vec3{static_cast<float>(side) * spacing + 20.0f, static_cast<float>(side) * spacing + 20.0f,
                          static_cast<float>(side) * spacing + 20.0f});
    sim.setSizeBox(sim.world().getWorldSize(), static_cast<int>(cellSize));
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.neighborList().setMode(NeighborListMode::Full);

    for (uint32_t idx : order) {
        sim.appendAtomFast(glm::vec3{base[idx].x, base[idx].y, base[idx].z}, glm::vec3{0.0f, 0.0f, 0.0f}, AtomData::Type::H, false);
    }
    sim.finalizeAtomBatch();
    sim.neighborList().build(sim.atoms(), sim.world());

    SceneData data;
    const AtomStorage& atoms = sim.atoms();
    const uint32_t n = static_cast<uint32_t>(atoms.size());
    data.mobileCount = static_cast<uint32_t>(atoms.mobileCount());
    data.totalCount = n;
    data.cutoff = sim.neighborList().cutoff();

    data.positions.resize(n * 4);
    data.types.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        data.positions[i * 4 + 0] = atoms.posX(i);
        data.positions[i * 4 + 1] = atoms.posY(i);
        data.positions[i * 4 + 2] = atoms.posZ(i);
        data.positions[i * 4 + 3] = 0.0f;
        data.types[i] = static_cast<uint32_t>(atoms.type(i));
    }
    data.nlOffsets = sim.neighborList().offsets();
    data.nlNeighbors = sim.neighborList().neighbors();
    return data;
}

struct ComputeUniforms {
    float cutoffSqr;
    float epsilon;
    uint32_t mobileCount;
    uint32_t typeCount;
};

// Standalone GPU LJ харнес: владеет pipeline + буферами, умеет batched dispatch
// без readback. Зеркалит bind layout physics_lj.wgsl.
class LJHarness {
public:
    void init() {
        wgpu::Device device = *WGPUContext::instance().device();

        std::array<wgpu::BindGroupLayoutEntry, 7> entries{};
        entries[0].binding = 0;
        entries[0].visibility = wgpu::ShaderStage::Compute;
        entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
        for (uint32_t i = 1; i <= 5; ++i) {
            entries[i].binding = i;
            entries[i].visibility = wgpu::ShaderStage::Compute;
            entries[i].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        }
        entries[6].binding = 6;
        entries[6].visibility = wgpu::ShaderStage::Compute;
        entries[6].buffer.type = wgpu::BufferBindingType::Storage;
        bindGroupLayout_ = WGPUContext::instance().createBindGroupLayout(entries, "LJReorder_BGL");

        wgpu::PipelineLayoutDescriptor plDesc{};
        plDesc.bindGroupLayoutCount = 1;
        plDesc.bindGroupLayouts = reinterpret_cast<WGPUBindGroupLayout*>(&bindGroupLayout_);
        wgpu::PipelineLayout layout = device.createPipelineLayout(plDesc);

        WGPUShaderSourceWGSL wgslDesc{};
        wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
        wgslDesc.code = wgpu::StringView(physics_ljWGSL);
        wgpu::ShaderModuleDescriptor smDesc{};
        smDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);
        wgpu::ShaderModule shader = device.createShaderModule(smDesc);

        wgpu::ComputePipelineDescriptor pDesc{};
        pDesc.layout = layout;
        pDesc.compute.module = shader;
        pDesc.compute.entryPoint = wgpu::StringView("compute_lj");
        pipeline_ = device.createComputePipeline(pDesc);

        uniformBuffer_ = WGPUContext::instance().createBuffer(
            sizeof(ComputeUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, "LJReorder_Uniforms");
    }

    void uploadScene(const SceneData& scene, const LJForceField& lj) {
        const wgpu::BufferUsage storage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        const uint32_t n = scene.totalCount;

        positions_ = WGPUContext::instance().createBuffer(n * sizeof(float) * 4, storage, "LJReorder_Pos");
        types_ = WGPUContext::instance().createBuffer(n * sizeof(uint32_t), storage, "LJReorder_Types");
        nlOffsets_ = WGPUContext::instance().createBuffer(scene.nlOffsets.size() * sizeof(uint32_t), storage, "LJReorder_Off");
        nlNeighbors_ = WGPUContext::instance().createBuffer(std::max<size_t>(scene.nlNeighbors.size(), 1) * sizeof(uint32_t),
                                                            storage, "LJReorder_Nbr");
        forces_ = WGPUContext::instance().createBuffer(n * sizeof(float) * 4, storage | wgpu::BufferUsage::CopySrc, "LJReorder_Forces");

        constexpr size_t kTypeCount = static_cast<size_t>(AtomData::Type::COUNT);
        std::vector<float> ljPacked(kTypeCount * kTypeCount * 2);
        for (size_t i = 0; i < kTypeCount; ++i) {
            const auto& row = lj.pairRow(static_cast<AtomData::Type>(i));
            for (size_t j = 0; j < kTypeCount; ++j) {
                ljPacked[(i * kTypeCount + j) * 2 + 0] = row[j].potentialC6;
                ljPacked[(i * kTypeCount + j) * 2 + 1] = row[j].potentialC12;
            }
        }
        ljPairs_ = WGPUContext::instance().createBuffer(ljPacked.size() * sizeof(float), storage, "LJReorder_LJPairs");

        auto q = WGPUContext::instance().queue();
        q->writeBuffer(*positions_, 0, scene.positions.data(), scene.positions.size() * sizeof(float));
        q->writeBuffer(*types_, 0, scene.types.data(), scene.types.size() * sizeof(uint32_t));
        q->writeBuffer(*nlOffsets_, 0, scene.nlOffsets.data(), scene.nlOffsets.size() * sizeof(uint32_t));
        if (!scene.nlNeighbors.empty()) {
            q->writeBuffer(*nlNeighbors_, 0, scene.nlNeighbors.data(), scene.nlNeighbors.size() * sizeof(uint32_t));
        }
        q->writeBuffer(*ljPairs_, 0, ljPacked.data(), ljPacked.size() * sizeof(float));

        ComputeUniforms u{};
        u.cutoffSqr = scene.cutoff * scene.cutoff;
        u.epsilon = 1e-6f;
        u.mobileCount = scene.mobileCount;
        u.typeCount = static_cast<uint32_t>(kTypeCount);
        q->writeBuffer(*uniformBuffer_, 0, &u, sizeof(u));

        std::array<wgpu::BindGroupEntry, 7> bg{};
        bg[0].binding = 0;
        bg[0].buffer = *uniformBuffer_;
        bg[0].size = sizeof(ComputeUniforms);
        bg[1].binding = 1;
        bg[1].buffer = *positions_;
        bg[1].size = n * sizeof(float) * 4;
        bg[2].binding = 2;
        bg[2].buffer = *types_;
        bg[2].size = n * sizeof(uint32_t);
        bg[3].binding = 3;
        bg[3].buffer = *nlOffsets_;
        bg[3].size = scene.nlOffsets.size() * sizeof(uint32_t);
        bg[4].binding = 4;
        bg[4].buffer = *nlNeighbors_;
        bg[4].size = std::max<size_t>(scene.nlNeighbors.size(), 1) * sizeof(uint32_t);
        bg[5].binding = 5;
        bg[5].buffer = *ljPairs_;
        bg[5].size = ljPacked.size() * sizeof(float);
        bg[6].binding = 6;
        bg[6].buffer = *forces_;
        bg[6].size = n * sizeof(float) * 4;
        bindGroup_ = WGPUContext::instance().createBindGroup(*bindGroupLayout_, bg, "LJReorder_BG");

        mobileCount_ = scene.mobileCount;
    }

    // K dispatch'ей в одном submit + один blocking poll. Возвращает после
    // завершения GPU. Fixed submit/poll overhead амортизируется по K.
    void dispatchBatch(int k) {
        wgpu::Device device = *WGPUContext::instance().device();
        wgpu::CommandEncoder encoder = device.createCommandEncoder({});
        wgpu::ComputePassEncoder pass = encoder.beginComputePass({});
        pass.setPipeline(*pipeline_);
        pass.setBindGroup(0, *bindGroup_, 0, nullptr);
        const uint32_t groups = (mobileCount_ + 63u) / 64u;
        for (int i = 0; i < k; ++i) {
            pass.dispatchWorkgroups(groups, 1, 1);
        }
        pass.end();
        wgpu::CommandBuffer cmd = encoder.finish({});
        WGPUContext::instance().queue()->submit(1, &cmd);
        device.poll(true, nullptr);
    }

private:
    wgpu::raii::BindGroupLayout bindGroupLayout_;
    wgpu::raii::ComputePipeline pipeline_;
    wgpu::raii::BindGroup bindGroup_;
    wgpu::raii::Buffer uniformBuffer_;
    wgpu::raii::Buffer positions_;
    wgpu::raii::Buffer types_;
    wgpu::raii::Buffer nlOffsets_;
    wgpu::raii::Buffer nlNeighbors_;
    wgpu::raii::Buffer ljPairs_;
    wgpu::raii::Buffer forces_;
    uint32_t mobileCount_ = 0;
};

void runReorderBench(benchmark::State& state, Ordering ordering) {
    benchmarkDevice(); // поднять WGPU device

    const int atomCount = static_cast<int>(state.range(0));
    SceneData scene = buildScene(atomCount, ordering, /*spacing=*/3.0f, /*cellSize=*/6.0f);
    LJForceField lj;

    LJHarness harness;
    harness.init();
    harness.uploadScene(scene, lj);

    harness.dispatchBatch(2); // warmup

    for (auto _ : state) {
        harness.dispatchBatch(kDispatchesPerSubmit);
    }
    // items = atoms × dispatch'ей за все итерации; per-dispatch время = wall/k.
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kDispatchesPerSubmit) * atomCount);
    state.counters["dispatches_per_iter"] = kDispatchesPerSubmit;
    state.counters["neighbors"] = static_cast<double>(scene.nlNeighbors.size()) / std::max<uint32_t>(scene.mobileCount, 1);
}

} // namespace

// @bench_meta {"id":"GpuLJReorder/Identity","ru":"GPU LJ kernel, исходный порядок","group":"Симуляция/GPU"}
void BM_GpuLJReorder_Identity(benchmark::State& state) { runReorderBench(state, Ordering::Identity); }
// @bench_meta {"id":"GpuLJReorder/Random","ru":"GPU LJ kernel, случайный порядок","group":"Симуляция/GPU"}
void BM_GpuLJReorder_Random(benchmark::State& state) { runReorderBench(state, Ordering::Random); }
// @bench_meta {"id":"GpuLJReorder/Morton","ru":"GPU LJ kernel, Morton порядок","group":"Симуляция/GPU"}
void BM_GpuLJReorder_Morton(benchmark::State& state) { runReorderBench(state, Ordering::Morton); }

BENCHMARK(BM_GpuLJReorder_Identity)->Arg(15625)->Arg(103823)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuLJReorder_Random)->Arg(15625)->Arg(103823)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuLJReorder_Morton)->Arg(15625)->Arg(103823)->Unit(benchmark::kMicrosecond);
