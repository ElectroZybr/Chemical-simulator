// Гейт Stage-1 zero-copy: read-only render-bind API резидентной GPU-физики
// (GpuResidentPhysics::positionsBuffer/velocitiesBuffer/renderBoundCount/
// renderBufferGeneration). Stage 1 — чистое добавление, поведение шага/NL не
// меняется; этот гейт проверяет КОНТРАКТ нового seam'а, который Stage 2 (рендер)
// будет биндить напрямую вместо per-draw upload.
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Падение/throw абортит прогон — как прочие BM_Gpu*-гейты.
//
// Что проверяем:
//   1. После uploadFromCpu: renderBoundCount() == totalCount() == число атомов;
//      positionsBuffer()/velocitiesBuffer() отдают валидный (non-null) handle с
//      размером >= totalCount*16 (формат array<vec4<f32>>, как ждёт шейдер).
//   2. renderBufferGeneration() РАСТЁТ при росте сцены, который пересоздаёт
//      резидентные pos/vel (totalCount > atomCapacity_, т.е. за пределами headroom).
//   3. renderBufferGeneration() НЕ растёт при re-upload ТОГО ЖЕ размера (буфера не
//      пересоздаются) — доказывает, что счётчик тикает ИМЕННО на пересоздании
//      pos/vel (§11 edit 3), а не на каждом upload.
//   4. Рост В ПРЕДЕЛАХ headroom (cap = n + n/2 + 1) не пересоздаёт буфера и не тикает
//      generation — подтверждает, что seam стабилен на типичном «+1 атом».

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

#include <webgpu/webgpu.hpp>

#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/World.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"
using namespace Lattice;

namespace {

[[noreturn]] void fail(const char* what) {
    std::printf("[FAIL] BM_GpuRenderBindApi: %s\n", what);
    std::fflush(stdout);
    throw std::runtime_error(what);
}

// Заливает в GpuResidentPhysics сцену из atomCount атомов в кубе box. Каждый
// upload строит свежий World+NL, как BM_GpuResidentNlBuild::checkScene.
void uploadScene(GpuResidentPhysics& grp, uint32_t atomCount, float box) {
    World world(Vec3f(box, box, box));
    AtomStorage& atoms = world.getAtomStorage();
    atoms.reserve(atomCount);
    for (uint32_t i = 0; i < atomCount; ++i) {
        const float t = static_cast<float>(i);
        // Детерминированная раскладка в пределах бокса (значения для гейта не важны —
        // проверяем контракт буферов, не физику).
        const float px = 1.0f + std::fmod(t * 1.7f, box - 2.0f);
        const float py = 1.0f + std::fmod(t * 2.3f, box - 2.0f);
        const float pz = 1.0f + std::fmod(t * 3.1f, box - 2.0f);
        atoms.addAtom(Vec3f(px, py, pz), Vec3f(0, 0, 0), AtomData::Type::H);
    }
    world.getGrid().rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());
    NeighborList& nl = world.getNeighborList();
    nl.setMode(NeighborListMode::Full);
    nl.setParams(5.0f, 1.0f);
    nl.build(atoms, world);

    LJForceField lj;
    grp.uploadFromCpu(atoms, nl, lj, box, box, box, 0.0f, 0.0f, 0.0f, /*ljEnabled=*/true,
                      /*coulombEnabled=*/false); // gravity=0 (render-bind API контракт, не траектория)
}

// Контракт буферов после upload под atomCount атомов.
void checkBufferContract(const GpuResidentPhysics& grp, uint32_t atomCount) {
    if (grp.renderBoundCount() != atomCount) {
        std::printf("[FAIL] renderBoundCount()=%u != atomCount=%u\n", grp.renderBoundCount(), atomCount);
        fail("renderBoundCount() != atomCount");
    }
    if (grp.renderBoundCount() != grp.totalCount()) {
        std::printf("[FAIL] renderBoundCount()=%u != totalCount()=%u\n", grp.renderBoundCount(), grp.totalCount());
        fail("renderBoundCount() != totalCount()");
    }
    const wgpu::Buffer pos = grp.positionsBuffer();
    const wgpu::Buffer vel = grp.velocitiesBuffer();
    if (!pos || !vel) {
        fail("positionsBuffer()/velocitiesBuffer() returned null handle");
    }
    const uint64_t needBytes = static_cast<uint64_t>(atomCount) * 16u; // array<vec4<f32>>
    if (pos.getSize() < needBytes || vel.getSize() < needBytes) {
        std::printf("[FAIL] buffer too small: pos=%llu vel=%llu need>=%llu\n", static_cast<unsigned long long>(pos.getSize()),
                    static_cast<unsigned long long>(vel.getSize()), static_cast<unsigned long long>(needBytes));
        fail("resident pos/vel buffer smaller than totalCount*16");
    }
}

void runRenderBindApi(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        GpuResidentPhysics grp;

        // (1) Стартовая малая сцена: контракт + базовая generation.
        uploadScene(grp, /*atomCount=*/4, /*box=*/40.0f);
        checkBufferContract(grp, 4);
        const uint64_t gen0 = grp.renderBufferGeneration();

        // (3) Re-upload ТОГО ЖЕ размера: буфера не пересоздаются (4 <= atomCapacity_),
        //     generation НЕ растёт.
        uploadScene(grp, 4, 40.0f);
        checkBufferContract(grp, 4);
        if (grp.renderBufferGeneration() != gen0) {
            std::printf("[FAIL] generation changed on same-size re-upload: %llu -> %llu\n",
                        static_cast<unsigned long long>(gen0), static_cast<unsigned long long>(grp.renderBufferGeneration()));
            fail("renderBufferGeneration() ticked on same-size re-upload (should tick only on pos/vel recreate)");
        }

        // (4) Рост В ПРЕДЕЛАХ headroom: первый upload(4) задаёт cap = 4 + 4/2 + 1 = 7.
        //     Рост до 6 (<=7) НЕ пересоздаёт буфера → generation НЕ растёт.
        uploadScene(grp, 6, 40.0f);
        checkBufferContract(grp, 6);
        if (grp.renderBufferGeneration() != gen0) {
            std::printf("[FAIL] generation changed on within-headroom grow (4->6, cap=7): %llu -> %llu\n",
                        static_cast<unsigned long long>(gen0), static_cast<unsigned long long>(grp.renderBufferGeneration()));
            fail("renderBufferGeneration() ticked within headroom (no recreate expected)");
        }

        // (2) Рост ЗА headroom: до 4000 атомов (>> cap=7) — резидентные pos/vel
        //     пересоздаются, generation ОБЯЗАН вырасти.
        uploadScene(grp, 4000, 200.0f);
        checkBufferContract(grp, 4000);
        if (grp.renderBufferGeneration() <= gen0) {
            std::printf("[FAIL] generation did NOT grow on scene grow past headroom (6->4000): %llu -> %llu\n",
                        static_cast<unsigned long long>(gen0), static_cast<unsigned long long>(grp.renderBufferGeneration()));
            fail("renderBufferGeneration() did not increment on pos/vel recreate");
        }

        // (5) КРОСС-ИНСТАНС уникальность (регресс на серьёзный баг): после GPU off→on
        //     создаётся НОВЫЙ GpuResidentPhysics, который может переиспользовать адрес
        //     старого. Его generation ОБЯЗАН быть глобально-уникальным (> предыдущего),
        //     иначе рендер-кэш bind-group спутает новые pos/vel буфера со старыми
        //     (ссылка на освобождённый буфер → устаревшие/мусорные атомы).
        const uint64_t genPrev = grp.renderBufferGeneration();
        {
            GpuResidentPhysics grp2; // имитация нового инстанса после setGpuMode(false)->true
            uploadScene(grp2, 4, 40.0f);
            checkBufferContract(grp2, 4);
            if (grp2.renderBufferGeneration() <= genPrev) {
                std::printf("[FAIL] new-instance generation NOT globally unique: prev=%llu new=%llu\n",
                            static_cast<unsigned long long>(genPrev),
                            static_cast<unsigned long long>(grp2.renderBufferGeneration()));
                fail("renderBufferGeneration() not globally monotonic across instances (stale bind-group risk)");
            }
        }

        std::printf("[ RENDER-BIND ] gen0=%llu genGrown=%llu boundCount=%u  CONTRACT OK (incl. cross-instance unique)\n",
                    static_cast<unsigned long long>(gen0), static_cast<unsigned long long>(grp.renderBufferGeneration()),
                    grp.renderBoundCount());
        std::fflush(stdout);
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuRenderBindApi/Contract","ru":"GPU resident render-bind API contract (Stage 1 zero-copy)","group":"Симуляция/GPU"}
void BM_GpuRenderBindApi_Contract(benchmark::State& state) { runRenderBindApi(state); }

BENCHMARK(BM_GpuRenderBindApi_Contract)->Iterations(1)->Unit(benchmark::kMicrosecond);
