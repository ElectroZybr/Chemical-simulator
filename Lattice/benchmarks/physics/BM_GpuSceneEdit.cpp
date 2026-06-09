// Регрессионные гейты для правки сцены под включённым GPU-режимом.
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Падение/паника абортит прогон.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h" // benchmarkDevice()
#include "Generators/Generators.h"  // MERGE: App/Scenes (namespace Scenes) -> Lattice/Generators (namespace Generators)
#include "Engine/Simulation.h"
#include <glm/glm.hpp>
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
using namespace Lattice;

namespace {

// Страж "атомы не исчезают": исчезновение на экране = позиция ушла в NaN/inf
// (рендер клиппит) либо число атомов поехало. Кидает, если найдена не-конечная
// позиция. (Текущие maxMove-проверки NaN не ловят: max(0, NaN) == NaN, а
// NaN < tol == false.)
void requireFiniteAtoms(const AtomStorage& a, const char* tag) {
    for (size_t i = 0; i < a.size(); ++i) {
        if (!std::isfinite(a.posX(i)) || !std::isfinite(a.posY(i)) || !std::isfinite(a.posZ(i))) {
            std::printf("[FAIL] non-finite position at atom %zu (%s)\n", i, tag);
            throw std::runtime_error("non-finite atom position (atom 'vanished')");
        }
    }
}

// Гейт 1: добавление атомов при включённом GPU должно пере-синкать
// резидентность (буфера растут под новый размер), а не переполнять буфера
// старого размера. До фикса: wgpuQueueWriteBuffer overrun + добавленные атомы
// не интегрировались. Проверка: добавленные атомы со скоростью реально
// двигаются (их интегрирует GPU после re-sync); до фикса они оставались на месте.
void runAddSceneWhileGpu(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld({120, 120, 120});
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setDt(0.01f);
        Generators::triangularBipyramidCrystal(sim, 8, AtomData::Type::Z);

        sim.setGpuMode(true);
        const size_t baseCount = sim.atoms().size();

        const size_t addStart = sim.atoms().size();
        for (int i = 0; i < 200; ++i) {
            const float x = 12.0f + 6.0f * static_cast<float>(i % 6);
            const float y = 12.0f + 6.0f * static_cast<float>((i / 6) % 6);
            const float z = 12.0f + 6.0f * static_cast<float>(i / 36);
            sim.createAtom(glm::vec3(x, y, z), glm::vec3(1.5f, 0.0f, 0.0f), AtomData::Type::Z, /*fixed=*/false);
        }
        const size_t newCount = sim.atoms().size();
        if (newCount <= baseCount) {
            throw std::runtime_error("BM_GpuSceneEdit setup: scene did not grow");
        }

        std::vector<float> startX(newCount - addStart);
        for (size_t i = addStart; i < newCount; ++i) {
            startX[i - addStart] = sim.atoms().posX(i);
        }

        for (int s = 0; s < 40; ++s) {
            sim.update();
            if (s % 4 == 0) {
                sim.syncFromGpuIfNeeded();
            }
        }
        sim.syncFromGpuIfNeeded();

        double maxMove = 0.0;
        for (size_t i = addStart; i < newCount; ++i) {
            maxMove = std::max(maxMove, std::abs(static_cast<double>(sim.atoms().posX(i)) - startX[i - addStart]));
        }
        std::printf("[ GPU-EDIT ] base=%zu added=%zu total=%zu maxMoveAdded=%.4f\n", baseCount, newCount - addStart,
                    newCount, maxMove);
        state.counters["added"] = static_cast<double>(newCount - addStart);
        state.counters["max_move_added"] = maxMove;

        requireFiniteAtoms(sim.atoms(), "gpu-edit");
        if (sim.atoms().size() != newCount) {
            throw std::runtime_error("atom count changed unexpectedly after GPU stepping");
        }

        if (maxMove < 1e-2) {
            throw std::runtime_error("GPU did not integrate atoms added after setGpuMode (scene edit not re-synced)");
        }

        sim.setGpuMode(false); // обратный toggle должен оставаться рабочим
    }
    state.SetItemsProcessed(state.iterations());
}

// Гейт 2: включение GPU на ПУСТОМ мире (0 атомов) — как при "добавить новый мир"
// + GPU — не должно падать. До фикса ensureCapacity(0,0,0) растил только
// nlOffsets, оставляя атомные буфера null -> rebuildBindGroups биндил null ->
// wgpu panic "invalid bind group entry" -> abort процесса. После: резервируем
// минимум 1 элемент. Гейт неявный (паника абортит прогон) + явная проверка, что
// атомы, добавленные в пустой GPU-мир, затем интегрируются.
void runEmptyWorldGpu(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld({120, 120, 120}); // пустой мир, 0 атомов
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setDt(0.01f);

        sim.setGpuMode(true); // <<< на 0 атомов — до фикса здесь panic+abort
        if (!sim.isGpuMode()) {
            throw std::runtime_error("BM_GpuSceneEdit: setGpuMode(true) on empty world did not engage");
        }
        for (int s = 0; s < 5; ++s) {
            sim.update(); // шаг пустого GPU-мира — no-op (0 рабочих групп)
        }
        sim.syncFromGpuIfNeeded(); // download 0 атомов

        // Теперь добавляем атомы со скоростью в пустой GPU-мир: re-sync с нуля
        // должен залить их и проинтегрировать.
        for (int i = 0; i < 64; ++i) {
            const float x = 12.0f + 6.0f * static_cast<float>(i % 4);
            const float y = 12.0f + 6.0f * static_cast<float>((i / 4) % 4);
            const float z = 12.0f + 6.0f * static_cast<float>(i / 16);
            sim.createAtom(glm::vec3(x, y, z), glm::vec3(1.5f, 0.0f, 0.0f), AtomData::Type::Z, /*fixed=*/false);
        }
        const size_t added = sim.atoms().size();
        std::vector<float> startX(added);
        for (size_t i = 0; i < added; ++i) {
            startX[i] = sim.atoms().posX(i);
        }
        for (int s = 0; s < 40; ++s) {
            sim.update();
            if (s % 4 == 0) {
                sim.syncFromGpuIfNeeded();
            }
        }
        sim.syncFromGpuIfNeeded();
        double maxMove = 0.0;
        for (size_t i = 0; i < added; ++i) {
            maxMove = std::max(maxMove, std::abs(static_cast<double>(sim.atoms().posX(i)) - startX[i]));
        }
        std::printf("[ GPU-EMPTY] added=%zu maxMove=%.4f\n", added, maxMove);
        state.counters["max_move"] = maxMove;
        requireFiniteAtoms(sim.atoms(), "gpu-empty-then-add");
        if (maxMove < 1e-2) {
            throw std::runtime_error("GPU did not integrate atoms added to an empty GPU world");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

// Гейт 3: мульти-мир + GPU. updateAll() шагает ВСЕ миры, поэтому GPU-мир,
// который не активен, продолжает считаться в VRAM. Но syncFromGpuIfNeeded()
// должен скачать его позиции в CPU для рендера — иначе неактивный GPU-мир
// "застывает" на экране (рендер читает устаревшую CPU-копию). Проверка: после
// шагов неактивный GPU-мир сдвинулся в CPU-хранилище. До фикса sync качал
// только активный мир -> неактивный GPU-мир заморожен.
void runMultiWorldGpuSync(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld({120, 120, 120}); // world 0 (активный)
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setDt(0.01f);
        // Малая скорость: за 40 шагов смещение ~0.12 < 0.5*skin -> NL rebuild НЕ
        // срабатывает, поэтому его downloadToCpu не маскирует баг и единственный
        // путь синка неактивного GPU-мира — syncFromGpuIfNeeded.
        for (int i = 0; i < 64; ++i) {
            const float x = 12.0f + 6.0f * static_cast<float>(i % 4);
            const float y = 12.0f + 6.0f * static_cast<float>((i / 4) % 4);
            const float z = 12.0f + 6.0f * static_cast<float>(i / 16);
            sim.createAtom(glm::vec3(x, y, z), glm::vec3(0.3f, 0.0f, 0.0f), AtomData::Type::Z, /*fixed=*/false);
        }
        sim.setGpuMode(true); // world 0 -> GPU (пока активен)

        const size_t n0 = sim.worldAt(0).getAtomStorage().size();
        std::vector<float> startX(n0);
        for (size_t i = 0; i < n0; ++i) {
            startX[i] = sim.worldAt(0).getAtomStorage().posX(i);
        }

        sim.createWorld({120, 120, 120}); // world 1
        sim.setActiveWorld(1);            // world 0 теперь GPU + НЕактивен

        for (int s = 0; s < 40; ++s) {
            sim.updateAll(); // шагает оба мира (world 0 на GPU)
            if (s % 4 == 0) {
                sim.syncFromGpuIfNeeded();
            }
        }
        sim.syncFromGpuIfNeeded();

        double maxMove = 0.0;
        for (size_t i = 0; i < n0; ++i) {
            maxMove = std::max(maxMove, std::abs(static_cast<double>(sim.worldAt(0).getAtomStorage().posX(i)) - startX[i]));
        }
        std::printf("[ GPU-MW   ] inactive-gpu-world maxMove=%.4f\n", maxMove);
        std::fflush(stdout);
        state.counters["max_move"] = maxMove;
        requireFiniteAtoms(sim.worldAt(0).getAtomStorage(), "gpu-multiworld-w0");
        if (maxMove < 1e-2) {
            throw std::runtime_error("Inactive GPU world frozen in CPU storage (syncFromGpuIfNeeded syncs only active world)");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

// Гейт 4 (C1): правка контента БЕЗ изменения числа атомов под включённым GPU.
// Скорость атому 0 (как делает ThermalTool через atoms()+notifySceneEdited)
// должна быть подхвачена. До фикса ресинк решался ТОЛЬКО по числу атомов —
// правка той же длины терялась, GPU шагал старые скорости. Две изолированные
// частицы (вне cutoff): без правки атом 0 стоит; с правкой едет ровно v*t.
void runSameCountEditWhileGpu(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld({120, 120, 120});
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setDt(0.01f);
        // Две частицы на 60 друг от друга (>> cutoff 5) — без взаимодействия,
        // поэтому смещение атома 0 определяется только заданной скоростью.
        sim.appendAtomFast(glm::vec3(30.0f, 60.0f, 60.0f), glm::vec3(0, 0, 0), AtomData::Type::Z);
        sim.appendAtomFast(glm::vec3(90.0f, 60.0f, 60.0f), glm::vec3(0, 0, 0), AtomData::Type::Z);
        sim.finalizeAtomBatch();

        sim.setGpuMode(true);
        const size_t n = sim.atoms().size();

        // Прогрев: GPU шагает, CPU-копия устаревает (cpuPositionsDirty).
        for (int s = 0; s < 12; ++s) {
            sim.update();
        }
        sim.syncFromGpuIfNeeded();
        const float baseX = sim.atoms().posX(0);

        // Правка БЕЗ изменения числа атомов (имитация ThermalTool).
        sim.syncGpuBeforeEdit();
        sim.atoms().vxData()[0] = 10.0f;
        sim.notifySceneEdited();

        for (int s = 0; s < 40; ++s) {
            sim.update();
            if (s % 4 == 0) {
                sim.syncFromGpuIfNeeded();
            }
        }
        sim.syncFromGpuIfNeeded();

        const double dx = static_cast<double>(sim.atoms().posX(0)) - baseX;
        std::printf("[ GPU-EDIT2] same-count velocity edit, atom0 dx=%.4f\n", dx);
        std::fflush(stdout);
        state.counters["atom0_dx"] = dx;
        requireFiniteAtoms(sim.atoms(), "gpu-samecount-edit");
        if (sim.atoms().size() != n) {
            throw std::runtime_error("atom count changed unexpectedly");
        }
        // vx=10 за 40*0.01=0.4 даёт ~+4 по x; без фикса (правка потеряна) ~0.
        if (dx < 1.0) {
            throw std::runtime_error("same-count velocity edit lost under GPU (scene version not tracked)");
        }

        sim.setGpuMode(false);
    }
    state.SetItemsProcessed(state.iterations());
}

// Гейт 5 (C1): удаление атома под GPU. Размер сцены уменьшается -> re-sync; при
// этом downloadToCpu должен выдержать момент, когда totalCount_ ещё старый, а
// AtomStorage уже меньше (clamp guard), без OOB/NaN. Проверка: финитность +
// корректное число атомов + продолжение шага оставшихся.
void runRemoveAtomWhileGpu(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld({120, 120, 120});
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setDt(0.01f);
        Generators::triangularBipyramidCrystal(sim, 8, AtomData::Type::Z);

        sim.setGpuMode(true);
        const size_t n = sim.atoms().size();
        if (n < 2) {
            throw std::runtime_error("BM_GpuSceneEdit setup: scene too small for remove");
        }
        for (int s = 0; s < 12; ++s) {
            sim.update();
        }

        sim.removeAtom(n - 1); // усечение под активным GPU
        if (sim.atoms().size() != n - 1) {
            throw std::runtime_error("removeAtom did not shrink storage");
        }
        sim.syncFromGpuIfNeeded(); // снимок выживших ПОСЛЕ ре-синка усечённой сцены
        const size_t survivors = sim.atoms().size();
        std::vector<float> beforeX(survivors);
        for (size_t i = 0; i < survivors; ++i) {
            beforeX[i] = sim.atoms().posX(i);
        }

        for (int s = 0; s < 40; ++s) {
            sim.update();
            if (s % 4 == 0) {
                sim.syncFromGpuIfNeeded();
            }
        }
        sim.syncFromGpuIfNeeded();

        double maxMove = 0.0;
        for (size_t i = 0; i < survivors; ++i) {
            maxMove = std::max(maxMove, std::abs(static_cast<double>(sim.atoms().posX(i)) - beforeX[i]));
        }
        std::printf("[ GPU-RM   ] removed 1: %zu->%zu, finite OK, survivor maxMove=%.4f\n", n, sim.atoms().size(), maxMove);
        std::fflush(stdout);
        state.counters["survivor_max_move"] = maxMove;
        requireFiniteAtoms(sim.atoms(), "gpu-remove");
        if (sim.atoms().size() != n - 1) {
            throw std::runtime_error("atom count drifted after removeAtom under GPU");
        }
        // Выжившие должны продолжать интегрироваться, а не замёрзнуть после
        // ре-синка усечённой сцены (LJ-силы в кристалле двигают их).
        if (maxMove < 1e-3) {
            throw std::runtime_error("survivors frozen after removeAtom re-sync under GPU");
        }

        sim.setGpuMode(false);
    }
    state.SetItemsProcessed(state.iterations());
}

// Гейт 6 (C1): resize бокса под GPU обновляет worldMax_ в VRAM. Дискриминирует
// именно распространение нового бокса: атом летит к СТАРОЙ стенке; после resize
// он должен пройти её и зайти в пространство, доступное только в новом боксе.
// Если version-фикс resize откатить, worldMax_ останется старым и confineToBox
// (integrate_verlet.wgsl) упрёт/отразит атом на старой границе. Число атомов не
// меняется — ловит только версия (size-проверка тут бессильна).
void runResizeWhileGpu(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld({120, 120, 120}); // worldMax = 120 - 1 = 119
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setDt(0.01f);
        // Атом летит к +x стенке; второй далеко (изоляция, нет LJ-помех).
        sim.appendAtomFast(glm::vec3(110.0f, 60.0f, 60.0f), glm::vec3(30.0f, 0, 0), AtomData::Type::Z);
        sim.appendAtomFast(glm::vec3(60.0f, 60.0f, 60.0f), glm::vec3(0, 0, 0), AtomData::Type::Z);
        sim.finalizeAtomBatch();

        sim.setGpuMode(true);
        const size_t n = sim.atoms().size();
        for (int s = 0; s < 8; ++s) {
            sim.update(); // прогрев: 110 -> ~112, старой стенки (119) не касается
        }

        sim.setSizeBox(glm::vec3(160.0f, 160.0f, 160.0f), 6); // worldMax 119 -> 159 (если версия подхватит)

        for (int s = 0; s < 40; ++s) {
            sim.update();
            if (s % 4 == 0) {
                sim.syncFromGpuIfNeeded();
            }
        }
        sim.syncFromGpuIfNeeded();

        const double x0 = sim.atoms().posX(0);
        std::printf("[ GPU-RSZ  ] resize 120->160 under GPU, atom0 x=%.4f\n", x0);
        std::fflush(stdout);
        state.counters["atom0_x"] = x0;
        requireFiniteAtoms(sim.atoms(), "gpu-resize");
        if (sim.atoms().size() != n) {
            throw std::runtime_error("atom count changed after resize under GPU");
        }
        // С обновлённым worldMax (159) атом проходит старую стенку и достигает
        // ~112 + 30*0.4 = 124. Со стале worldMax (119) — упирается/отражается у 119.
        if (x0 < 120.0) {
            throw std::runtime_error("resize did not propagate worldMax to GPU (atom confined to old box bound)");
        }

        sim.setGpuMode(false);
    }
    state.SetItemsProcessed(state.iterations());
}

// Общий сценарий: правка под РАБОТАЮЩИМ GPU, когда CPU-копия устарела (GPU ушёл
// вперёд, синка не было). Версия/размер расходятся -> uploadSceneToGpu, который
// ОБЯЗАН слить прогресс GPU перед перезаливкой, иначе атом 0 откатится к
// устаревшему CPU-снимку. Возвращает, насколько атом 0 продвинулся ОТНОСИТЕЛЬНО
// устаревшего снимка: >~0.3 значит прогресс сохранён, ~0 значит откат.
template <typename EditFn>
double atom0AdvanceAfterEditUnderDirtyGpu(EditFn&& applyEdit) {
    Simulation sim;
    sim.createWorld({120, 120, 120});
    sim.setLJEnabled(true);
    sim.setCoulombEnabled(false);
    sim.setBondFormationEnabled(false);
    sim.setDt(0.01f);
    // Медленный изолированный атом 0: скорость мала, чтобы за прогрев не сработал
    // displacement-rebuild (он бы синкнул CPU и убрал stale-окно), второй далеко.
    sim.appendAtomFast(glm::vec3(30.0f, 60.0f, 60.0f), glm::vec3(0.5f, 0, 0), AtomData::Type::Z);
    sim.appendAtomFast(glm::vec3(90.0f, 60.0f, 60.0f), glm::vec3(0, 0, 0), AtomData::Type::Z);
    sim.finalizeAtomBatch();

    sim.setGpuMode(true);
    for (int s = 0; s < 60; ++s) {
        sim.update(); // прогрев БЕЗ синка: GPU уходит вперёд (~+0.3), CPU устаревает
    }
    const float xStale = sim.atoms().posX(0); // устаревший CPU-снимок (~исходный 30)

    applyEdit(sim); // правка под dirty GPU (append без clear / cutoff)

    for (int s = 0; s < 8; ++s) {
        sim.update();
    }
    sim.syncFromGpuIfNeeded();
    requireFiniteAtoms(sim.atoms(), "edit-after-dirty");
    return static_cast<double>(sim.atoms().posX(0)) - static_cast<double>(xStale);
}

// Гейт 7 (C1): append к работающему GPU БЕЗ clear, когда CPU устарел. Старые
// атомы не должны откатиться к устаревшему снимку (uploadSceneToGpu сливает
// прогресс GPU перед перезаливкой; clamp оставляет свежий хвост из CPU).
void runAppendAfterDirtyGpu(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        const double advance = atom0AdvanceAfterEditUnderDirtyGpu([](Simulation& sim) {
            sim.appendAtomFast(glm::vec3(60.0f, 60.0f, 60.0f), glm::vec3(0, 0, 0), AtomData::Type::Z); // append без clear
            sim.finalizeAtomBatch();
        });
        std::printf("[ GPU-APP  ] append-after-dirty, atom0 advance=%.4f\n", advance);
        std::fflush(stdout);
        state.counters["atom0_advance"] = advance;
        // С merge'ом: ~0.3 (прогрев) + 0.04 (post) ~= 0.34. С откатом: ~0.04.
        if (advance < 0.2) {
            throw std::runtime_error("old atom rolled back to stale snapshot after append under GPU");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

// Гейт 8 (C1): смена cutoff под работающим GPU, когда CPU устарел. Тот же откат-
// риск: setNeighborListCutoff бампит версию без собственного синка. Атом 0
// изолирован, поэтому cutoff не влияет на его траекторию — ловим только откат.
void runCutoffAfterDirtyGpu(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        const double advance = atom0AdvanceAfterEditUnderDirtyGpu([](Simulation& sim) {
            sim.setNeighborListCutoff(4.0f); // listRadius 5 <= cellSize 6 — валидно
        });
        std::printf("[ GPU-CUT  ] cutoff-after-dirty, atom0 advance=%.4f\n", advance);
        std::fflush(stdout);
        state.counters["atom0_advance"] = advance;
        if (advance < 0.2) {
            throw std::runtime_error("old atom rolled back to stale snapshot after cutoff change under GPU");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

// Гейт 9 (C1): append MOBILE-атома, когда в сцене есть FIXED-атом, под dirty GPU.
// AtomStorage держит mobile-атомы сплошным префиксом: addAtom(fixed=false)
// вставляет новый mobile в позицию mobileCount_ через swap, сдвигая бывший
// fixed-слот в хвост. Значит "новый атом в хвосте" неверно, и слепая merge-
// выгрузка старого префикса перезатёрла бы новый mobile старым fixed-слотом.
// Новый атом (уникальная позиция x=15) должен сохраниться, а не получить ~100.
void runAppendMobileAmongFixedGpu(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        Simulation sim;
        sim.createWorld({120, 120, 120});
        sim.setLJEnabled(true);
        sim.setCoulombEnabled(false);
        sim.setBondFormationEnabled(false);
        sim.setDt(0.01f);
        // mobile (idx0) + fixed (idx1). Все изолированы (>> cutoff).
        sim.appendAtomFast(glm::vec3(30.0f, 60.0f, 60.0f), glm::vec3(0.5f, 0, 0), AtomData::Type::Z, /*fixed=*/false);
        sim.appendAtomFast(glm::vec3(100.0f, 60.0f, 60.0f), glm::vec3(0, 0, 0), AtomData::Type::Z, /*fixed=*/true);
        sim.finalizeAtomBatch();

        sim.setGpuMode(true);
        for (int s = 0; s < 60; ++s) {
            sim.update(); // dirty: GPU ушёл вперёд, CPU устарел
        }

        // Новый MOBILE на уникальной x=15: addAtom вставит его в середину
        // (swap с fixed@100), новый mobile окажется по индексу mobileCount_=1.
        sim.appendAtomFast(glm::vec3(15.0f, 60.0f, 60.0f), glm::vec3(0, 0, 0), AtomData::Type::Z, /*fixed=*/false);
        sim.finalizeAtomBatch();

        for (int s = 0; s < 8; ++s) {
            sim.update();
        }
        sim.syncFromGpuIfNeeded();

        requireFiniteAtoms(sim.atoms(), "gpu-append-among-fixed");
        double minDistTo15 = 1e9;
        for (size_t i = 0; i < sim.atoms().size(); ++i) {
            minDistTo15 = std::min(minDistTo15, std::abs(static_cast<double>(sim.atoms().posX(i)) - 15.0));
        }
        std::printf("[ GPU-AMF  ] append mobile among fixed, min|x-15|=%.4f\n", minDistTo15);
        std::fflush(stdout);
        state.counters["min_dist_to_15"] = minDistTo15;
        // Новый атом должен быть у своей позиции 15, а не перезатёрт fixed-слотом (~100).
        if (minDistTo15 > 1.0) {
            throw std::runtime_error("new mobile atom clobbered by prefix-merge (swap puts it among fixed, not at tail)");
        }

        sim.setGpuMode(false);
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuSceneEdit/AddAtomsResyncs","ru":"GPU: правка сцены ре-синкает","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_AddAtomsResyncs(benchmark::State& state) { runAddSceneWhileGpu(state); }

// @bench_meta {"id":"GpuSceneEdit/EmptyWorldEnable","ru":"GPU: включение на пустом мире","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_EmptyWorldEnable(benchmark::State& state) { runEmptyWorldGpu(state); }

// @bench_meta {"id":"GpuSceneEdit/MultiWorldSync","ru":"GPU: синк неактивного мира","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_MultiWorldSync(benchmark::State& state) { runMultiWorldGpuSync(state); }

// @bench_meta {"id":"GpuSceneEdit/SameCountEdit","ru":"GPU: правка той же длины ре-синкает","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_SameCountEdit(benchmark::State& state) { runSameCountEditWhileGpu(state); }

// @bench_meta {"id":"GpuSceneEdit/RemoveAtom","ru":"GPU: удаление атома","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_RemoveAtom(benchmark::State& state) { runRemoveAtomWhileGpu(state); }

// @bench_meta {"id":"GpuSceneEdit/Resize","ru":"GPU: resize бокса","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_Resize(benchmark::State& state) { runResizeWhileGpu(state); }

// @bench_meta {"id":"GpuSceneEdit/AppendAfterDirty","ru":"GPU: append без отката прогресса","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_AppendAfterDirty(benchmark::State& state) { runAppendAfterDirtyGpu(state); }

// @bench_meta {"id":"GpuSceneEdit/CutoffAfterDirty","ru":"GPU: cutoff без отката прогресса","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_CutoffAfterDirty(benchmark::State& state) { runCutoffAfterDirtyGpu(state); }

// @bench_meta {"id":"GpuSceneEdit/AppendMobileAmongFixed","ru":"GPU: новый mobile среди fixed","group":"Симуляция/GPU"}
void BM_GpuSceneEdit_AppendMobileAmongFixed(benchmark::State& state) { runAppendMobileAmongFixedGpu(state); }

BENCHMARK(BM_GpuSceneEdit_AddAtomsResyncs)->Iterations(1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuSceneEdit_EmptyWorldEnable)->Iterations(1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuSceneEdit_MultiWorldSync)->Iterations(1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuSceneEdit_SameCountEdit)->Iterations(1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuSceneEdit_RemoveAtom)->Iterations(1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuSceneEdit_Resize)->Iterations(1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuSceneEdit_AppendAfterDirty)->Iterations(1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuSceneEdit_CutoffAfterDirty)->Iterations(1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_GpuSceneEdit_AppendMobileAmongFixed)->Iterations(1)->Unit(benchmark::kMicrosecond);
