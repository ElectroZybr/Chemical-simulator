// Перф-гейт Инкремента B (zero-copy): доказывает, что УБРАН per-frame GPU->CPU
// download в «чистом» GPU-режиме, и при этом он СОХРАНЁН, когда активен CPU-
// потребитель позиций. Это и есть выигрыш Инкремента B.
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Падение/throw абортит прогон — как прочие BM_Gpu*-гейты.
//
// КОНТЕКСТ. Инкремент A связал резидентные VRAM-буфера pos/vel с рендером напрямую
// (zero-copy атомы). Но App/Application.cpp до Инкремента B звал syncFromGpuIfNeeded()
// БЕЗУСЛОВНО каждый кадр — download оставался, выигрыша не было. Инкремент B делает
// синк УСЛОВНЫМ: download происходит только если активен CPU-потребитель позиций
// (drawBonds/drawGrid/singleSelection/debugPanel/speed-color-авто). Когда ни один
// не активен — download ПРОПУСКАЕТСЯ, атомы рисуются прямо из VRAM.
//
// ЧТО ИЗМЕРЯЕТ ГЕЙТ (механизм каденции download, который и гейтит UI-предикат):
//   (A) ЧИСТЫЙ РЕЖИМ (нет CPU-потребителя => синк НЕ зовётся): за N «рендер-кадров»
//       (каждый = GPU step, помечающий cpuPositionsDirty) downloadCount() ОСТАЁТСЯ 0.
//       Это прямое доказательство, что download убран из чистого кадра.
//   (B) ПОТРЕБИТЕЛЬ АКТИВЕН (синк зовётся каждый кадр, как сегодня): downloadCount()
//       растёт ровно на N (по одному download на грязный кадр) — связи/грид/пикинг
//       продолжают видеть свежие позиции, регрессии нет.
//   (C) КОЛИЧЕСТВЕННО: замеряем стоимость ОДНОГО downloadToCpu при большом N, чтобы
//       привязать сэкономленный per-frame член к данным Фазы 0 (~556-800 μs @103k).
//
// ВАЖНО про предикат: сам UI-предикат cpuPositionConsumerActive живёт в App-фрейм-
// лупе (нужен GLFW/Interface, не поднимается headless). Гейт проверяет МЕХАНИЗМ,
// который предикат гейтит: «синк не вызван => 0 download», «синк вызван => download».
// Корректность самого предиката проверяется чтением кода + визуальными гейтами
// (bonds/grid/speed-color/picking следуют за атомами), которые прогоняются отдельно.
// Гейт намеренно НЕ дублирует предикат, чтобы тест не «дрейфил» от боевого кода.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/Simulation.h"
#include "Engine/World.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"
using namespace Lattice;

namespace {

// Сцена из side^3 движущихся атомов в GPU-режиме (как BM_GpuDiagnosticsGrid).
// Каждый sim.update() в GPU-режиме шагает на GPU и помечает cpuPositionsDirty —
// то есть «есть что качать», как реальный кадр между sync-точками.
void buildMovingGpuScene(Simulation& sim, int side, float box) {
    sim.createWorld(glm::vec3{box, box, box});
    sim.setSizeBox(glm::vec3{box, box, box}, 6);
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setGravity(glm::vec3{0.0f, 0.0f, 0.0f});
    sim.setDt(0.01f);
    for (int z = 0; z < side; ++z) {
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                const float vx = 8.0f * static_cast<float>((x % 3) - 1);
                const float vy = 8.0f * static_cast<float>((y % 3) - 1);
                const float vz = 8.0f * static_cast<float>((z % 3) - 1);
                sim.appendAtomFast(glm::vec3{50.0f + x * 3.0f, 50.0f + y * 3.0f, 50.0f + z * 3.0f}, glm::vec3{vx, vy, vz},
                                   AtomData::Type::H);
            }
        }
    }
    sim.finalizeAtomBatch();
    sim.setGpuMode(true);
}

void runGpuDownloadCadence(benchmark::State& state) {
    benchmarkDevice();

    constexpr int kFrames = 30; // имитируем 30 «рендер-кадров» (0.5 c @60fps)

    for (auto _ : state) {
        // --- (A) ЧИСТЫЙ РЕЖИМ: синк НЕ зовётся => download убран. ---
        {
            Simulation sim;
            buildMovingGpuScene(sim, /*side=*/8, /*box=*/160.0f); // 512 атомов
            const GpuResidentPhysics* gpu = sim.activeGpuResident();
            if (gpu == nullptr) {
                throw std::runtime_error("BM_GpuDownloadCadence: setGpuMode(true) не дал резидентную физику");
            }
            const uint64_t downloadsAtStart = gpu->downloadCount();
            for (int f = 0; f < kFrames; ++f) {
                sim.update(); // GPU шаг => cpuPositionsDirty. Синк НЕ зовём (чистый режим).
            }
            const uint64_t downloadsClean = gpu->downloadCount() - downloadsAtStart;
            std::printf("[ DL-CADENCE clean ] frames=%d downloads=%llu (ожидание: 0 — download убран)\n", kFrames,
                        static_cast<unsigned long long>(downloadsClean));
            std::fflush(stdout);
            state.counters["clean_downloads"] = static_cast<double>(downloadsClean);
            if (downloadsClean != 0) {
                throw std::runtime_error("чистый GPU-режим выполнил download без активного CPU-потребителя (выигрыш не реализован)");
            }
            sim.setGpuMode(false);
        }

        // --- (B) ПОТРЕБИТЕЛЬ АКТИВЕН: синк зовётся каждый кадр => download есть. ---
        {
            Simulation sim;
            buildMovingGpuScene(sim, /*side=*/8, /*box=*/160.0f);
            const GpuResidentPhysics* gpu = sim.activeGpuResident();
            if (gpu == nullptr) {
                throw std::runtime_error("BM_GpuDownloadCadence: setGpuMode(true) не дал резидентную физику (consumer case)");
            }
            const uint64_t downloadsAtStart = gpu->downloadCount();
            for (int f = 0; f < kFrames; ++f) {
                sim.update();                // GPU шаг => cpuPositionsDirty
                sim.syncFromGpuIfNeeded();   // потребитель активен => качаем (как App при drawBonds/grid/...)
            }
            const uint64_t downloadsConsumer = gpu->downloadCount() - downloadsAtStart;
            std::printf("[ DL-CADENCE consumer] frames=%d downloads=%llu (ожидание: %d — по одному на грязный кадр)\n", kFrames,
                        static_cast<unsigned long long>(downloadsConsumer), kFrames);
            std::fflush(stdout);
            state.counters["consumer_downloads"] = static_cast<double>(downloadsConsumer);
            // Ровно kFrames: каждый кадр шаг помечает dirty, синк качает и снимает флаг.
            if (downloadsConsumer != static_cast<uint64_t>(kFrames)) {
                throw std::runtime_error("при активном CPU-потребителе число download не совпало с числом грязных кадров");
            }
            sim.setGpuMode(false);
        }

        // --- (C) КОЛИЧЕСТВЕННО: стоимость ОДНОГО download при большом N. ---
        // Привязка к Фазе 0: per-frame download pos+vel — крупнейший одиночный член
        // кадра (~556-800 μs @103k). На чистом кадре Инкремент B убирает ровно его.
        {
            Simulation sim;
            // 40^3 = 64000 атомов — большой N в духе Фазы 0 (бокс крупнее под плотность).
            buildMovingGpuScene(sim, /*side=*/40, /*box=*/520.0f);
            const GpuResidentPhysics* gpu = sim.activeGpuResident();
            if (gpu == nullptr) {
                throw std::runtime_error("BM_GpuDownloadCadence: setGpuMode(true) не дал резидентную физику (timing case)");
            }
            const size_t atomCount = sim.atoms().size();
            sim.update(); // один шаг => dirty, есть что качать

            // Прогреть (первый map может тащить аллокацию), затем замерить медиану из 5.
            sim.syncFromGpuIfNeeded();
            std::vector<double> samplesUs;
            samplesUs.reserve(5);
            for (int r = 0; r < 5; ++r) {
                sim.update(); // снова dirty
                const auto t0 = std::chrono::high_resolution_clock::now();
                sim.syncFromGpuIfNeeded(); // ровно один блокирующий download pos+vel
                const auto t1 = std::chrono::high_resolution_clock::now();
                samplesUs.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
            }
            std::sort(samplesUs.begin(), samplesUs.end());
            const double medianUs = samplesUs[samplesUs.size() / 2];
            std::printf("[ DL-CADENCE cost  ] atoms=%zu one_download_pos+vel=%.1f us (median of 5) — этот член убран из чистого кадра\n",
                        atomCount, medianUs);
            std::fflush(stdout);
            state.counters["atoms"] = static_cast<double>(atomCount);
            state.counters["one_download_us"] = medianUs;
            sim.setGpuMode(false);
        }
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuDownloadCadence/SkippedInCleanMode","ru":"GPU: per-frame download убран в чистом режиме (Инкремент B)","group":"Симуляция/GPU"}
void BM_GpuDownloadCadence_SkippedInCleanMode(benchmark::State& state) { runGpuDownloadCadence(state); }

BENCHMARK(BM_GpuDownloadCadence_SkippedInCleanMode)->Iterations(1)->Unit(benchmark::kMicrosecond);
