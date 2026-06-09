# Benchmark results

Зафиксированные числа выигрышей по сериям коммитов на ветке
`optimization-research` поверх upstream `fd28dc8`.

> Замеры сделаны в **ДВУХ эпохах** — граница это merge-коммит `7a641ccc` (рефакторинг
> upstream/main на форк, 3-библиотечная раскладка `Lattice ← Rendering ← App`):
> - **ДО рефактора** — все разделы ниже (C/D и GPU-фазы 2.x) сняты на старой плоской раскладке,
>   на коммитах **перед** `7a641ccc`; цитируемые в них хеши (`a72aff1`, `722d0c4`, …) живы в истории.
> - **ПОСЛЕ рефактора** — раздел «Пост-мерж» в конце, переснят на новой 3-библиотечной раскладке,
>   на коммитах **после** `7a641ccc` (P1 `e6b09332` и далее).
>
> Две эпохи не сравнивать как back-to-back: между ними сменилась раскладка и часть инфраструктуры.

## Configuration

- **CPU**: Intel Core i7-14700K (20 cores / 28 threads)
- **GPU**: NVIDIA RTX 4070 SUPER (только для render-бенчей)
- **OS**: Windows 11 Pro 10.0.26100
- **Compiler**: clang 22.1.6 (`x86_64-pc-windows-msvc`)
- **Build**: `cmake --preset bench` (Release + `OPTIMIZE_FOR_NATIVE=ON` + `USE_TBB=ON`)
- **Flags**: `-O3 -march=native -DNDEBUG`
- **Bench framework**: GoogleBenchmark v1.9.5
- **Methodology**: `--benchmark_repetitions=3 --benchmark_min_time=0.2-0.3s --benchmark_report_aggregates_only=true`, числа = median

## Сводное сравнение: до оптимизаций / CPU / GPU

Главная метрика — время вычисления LJ-сил на шаг, одинаковый бенч во всех
трёх столбцах. «До оптимизаций» — upstream `fd28dc8` (single-thread, Half NL,
без cutoff-фильтра). «CPU оптимизированный» — main (C1 cutoff + D5 TBB parallel
Full NL). «GPU» — чистое время LJ-kernel на ветке `gpu-compute`.

Провенанс:
- CPU-столбцы: `SimulationFixture/ComputeForcesWithNeighborList` (pair LJ force,
  без интегрирования и NL rebuild). «До» — из `.scratch/baseline-clang-20260528.json`;
  «CPU опт» — свежий прогон main-ветки.
- GPU-столбец: `BM_GpuLJReorder_Identity` (естественный порядок атомов),
  per-dispatch = wall / 50 (батч 50 dispatch на один submit, kernel-only через
  `Rendering/shaders/physics_lj.wgsl`).
- Все: i7-14700K, RTX 4070 SUPER, clang 22.1.6 Release, median.

| N атомов | До оптимизаций | CPU оптимизированный | GPU (kernel) |
|---|---|---|---|
| 1 000 | 42.6 μs | 34.3 μs | — (ниже полезного диапазона GPU) |
| 15 625 | 742 μs | 195 μs | 12.6 μs |
| 103 823 | 5 120 μs | 981 μs | 83 μs |

Ускорения @ 103 823: CPU опт vs upstream **5.2×**; GPU vs CPU опт **11.8×**;
GPU vs upstream **62×**. @ 15 625: соответственно 3.8× / 15.5× / 59×.

Резидентный GPU full-step (predict + confine + LJ + correct, без per-step
readback, `BM_GpuFullStep`, per-step = wall / 20):

| N атомов | CPU steady-state шаг (~force+correct) | GPU резидентный шаг |
|---|---|---|
| 15 625 | ~205 μs | ~25 μs (~8×) |
| 103 823 | ~1 040 μs | ~107 μs (~9×) |

Полный per-frame GPU-замер с rebuild-каденцией (`BM_GpuFullStep_WithRebuild`,
реальный путь `Simulation` в GPU-режиме, та же методология 20 шагов + drain;
горячая сцена — когерентный дрейф vx=5, та же машина RTX 4070 SUPER):

| N атомов | GPU резидентный (без rebuild) | GPU реальный (с rebuild-round-trip) | каденция rebuild |
|---|---|---|---|
| 15 625 | ~25 μs | ~1000-1250 μs (**~40-50× медленнее**) | каждые ~7.5-8.5 шагов |
| 103 823 | ~107 μs | ~2150 μs (**~20× медленнее**) | каждые ~12 шагов |

Это и есть perf-корень «CPU отъедает у GPU»: резидентный пайплайн рвут две
CPU-синхронизации — disp-check (`maxDisplacementSqr` делает GPU-dispatch +
блокирующий poll КАЖДЫЕ 4 шага, даже без rebuild) и сам rebuild-round-trip
(downloadToCpu poll + CPU `rebuildPipeline` + uploadNeighborList при превышении
порога смещения). На динамичной сцене это съедает 20-50× чистого GPU-выигрыша.
Лечение — перенос disp-check и построения NL/сетки на GPU (cell-list/prefix-sum)
без poll'а. Замер sync-dominated и шумный (cv высокий), числа порядковые.

Шаг 1 оптимизации — disp-check сделан асинхронным (неблокирующий readback +
hard backstop вместо `dev.poll(true)` каждые 4 шага). `BM_GpuFullStep_WithRebuild`
параметризован числом шагов на кадр (steps_per_frame), публикует счётчики
async-consume / backstop (RTX 4070 SUPER):

| N / шагов-на-кадр | per-step | backstop / begins |
|---|---|---|
| 15 625 / 1 | ~804 μs | 0 / 186 |
| 15 625 / 2 | ~836 μs | 0 / 224 |
| 15 625 / 8 | ~739 μs | 109 / 201 |
| 15 625 / 20 | ~1123 μs | 295 / 379 |
| 103 823 / 2 | ~2562 μs | 0 / 50 |
| 103 823 / 8 | ~2244 μs | 44 / 51 |

При пейсинге приложения (1-2 шага на синк) backstop нулевой — disp-check
полностью асинхронен, stall'а нет. Прежний фиксированный замер 20-шагов-на-синк
загонял ~78% disp-check'ов в блокирующий backstop (GPU не успевал между шагами),
из-за чего async казался безвыигрышным — артефакт методики, не свойство фикса.
Чистый выигрыш от снятия disp-poll скромен на горячих сценах (доминирует
rebuild-round-trip), заметнее на спокойных, где перестройка редка. Сам
rebuild-round-trip (доминанта) — цель Шага 2 (построение NL на GPU).

## Шаг 2 — построение списка соседей целиком на GPU (round-trip убран)

Шаг 2 переносит саму перестройку списка соседей на GPU: cell-list (counting sort +
Blelloch exclusive scan + scatter) → Full NeighborList (count → scan → write) строится
из резидентных позиций прямо в резидентные буфера, которые читает LJ-ядро. Горячий путь
больше НЕ делает CPU round-trip (downloadToCpu + CPU rebuildPipeline + uploadNeighborList)
— именно он был доминантой замедления.

BEFORE/AFTER одного и того же `BM_GpuFullStep_WithRebuild` (реальный путь Simulation
в GPU-режиме, та же методология median-3, та же машина back-to-back; BEFORE = коммит
8780aba с CPU round-trip, AFTER = ae329ea с GPU-построением):

| N / шагов-на-кадр | BEFORE (CPU round-trip) | AFTER (GPU build) | ускорение | AFTER cv |
|---|---|---|---|---|
| 15 625 / 1 | 1172 μs | 234 μs | **5.0×** | 4.6% |
| 15 625 / 2 | 2777 μs | 332 μs | **8.4×** | 3.4% |
| 15 625 / 8 | 7842 μs | 835 μs | **9.4×** | 1.6% |
| 15 625 / 20 | 28 027 μs | 2181 μs | **12.8×** | 2.5% |
| 103 823 / 2 | 5754 μs | 1371 μs | **4.2×** | 4.9% |
| 103 823 / 8 | 20 174 μs | 3898 μs | **5.2×** | 2.5% |

Провенанс: процедура — `BM_GpuFullStep_WithRebuild` (`Benchmarks/physics/BM_GpuFullStep.cpp`),
метрика = per-iteration real_time (1 итерация = N шагов на кадр + один drain-sync);
код пути — rebuild-lambda в `Simulation::updateStateGpu` (`Engine/Simulation.cpp`),
`GpuResidentPhysics::rebuildNeighborListOnGpu` (`Engine/physics/gpu/GpuResidentPhysics.cpp`),
`GpuNeighborListBuilder` + `Rendering/shaders/gpu_cell_list.wgsl`; входы — когерентная
горячая сцена (дрейф vx=5), N = 15625 и 103823, шагов-на-кадр = 1/2/8/20, медиана 3
прогонов, `--benchmark_min_time=0.3s`; машина — та же (i7-14700K, RTX 4070 SUPER, clang
Release).

Ускорение 4–13× амортизированное — это НЕ per-rebuild-шаг 20-50× из раздела выше:
матрица амортизирует перестройки по всем шагам кадра, а GPU-построение само имеет
стоимость. AFTER стабилен (cv 1.5-5%, против шумного BEFORE round-trip); точный
множитель плавает с состоянием машины (ранний single-run давал 2.6-7.9×), но порядок
(несколько-× … ~10×) робастен. На 8/20 шагах часть disp-check'ов уходит в backstop
(быстрый submit-цикл бенча глушит async — артефакт методики, как в Шаге 1), но
round-trip убран на всех путях.

Корректность и свежесть (отдельные гейты, не таблица времени):
- `BM_GpuCorrectness`: GPU-траектория == CPU в пределах tolerance (max|Δ| = 0; сцена без
  rebuild — изолирует integrator+LJ).
- `BM_GpuNlFreshness`: на РЕЗИДЕНТНОМ GPU-списке под быстрым движением потерянных
  within-cutoff пар нет (MISSING = 0) — GPU-перестройка по disp-каденции свежа.
- `BM_GpuResidentNlBuild`: резидентный список поатомно == CPU Full (8 сцен, включая
  overflow-рост буфера).
- `BM_GpuDiagnosticsGrid`: CPU SpatialGrid диагностик (виз-сетка/overlay/панель)
  перебиннивается на каденции рендера и совпадает с позициями (одиночный + неактивный
  GPU-мир) — иначе после переноса перестройки на GPU замороженный CPU-грид давал бы
  «сетку, отделяющуюся от частиц».

## Профилирование GPU-кадра (measure-first, для решения о zero-copy)

Разложение per-frame стоимости GPU-режима на компоненты (`BM_GpuFrameBreakdown`,
аддитивный бенч, поведение не меняет; медиана 3, та же машина) — чтобы решить ПО ДАННЫМ,
оправдана ли будущая оптимизация «zero-copy рендер» (убрать per-frame скачивание позиций
GPU→CPU; рендер читал бы резидентные GPU-буфера напрямую).

| Компонент (на кадр) | N=15625 | N=103823 | Кратность/кадр |
|---|---|---|---|
| Шаг физики (predict+LJ+correct) | ~43 μs/шаг | ~111 μs/шаг | 1–2 шага |
| Перестройка NL (per-call) | ~530 μs | ~765 μs | раз в ~8 шагов (аморт. ~90–180 μs) |
| **Download поз+скор (цель zero-copy)** | ~130 μs | **~556–800 μs** | 1/кадр |
| Render pack+upload поз+скор | ~65 μs | ~560 μs | 1/draw |
| refreshDiagnosticsGrid (только если drawGrid) | ~64 μs | ~430 μs | 1/кадр усл. |

Провенанс: `Benchmarks/physics/BM_GpuFrameBreakdown.cpp` (компоненты шаг/rebuild/download/
refreshGrid — bench-owned `GpuResidentPhysics` через public API; render-upload реплицирует
pack+writeBuffer `RendererWGPU::drawAtomsImpl`); медиана 3, min_time 0.3s; i7-14700K /
RTX 4070 SUPER / clang Release. Числа шумные (cv до ~20%), но порядок робастен.

Наблюдение: на больших N **per-frame download (поз+скор) — крупнейший одиночный член кадра**
(≈5–7× одного шага физики), а вместе с render pack+upload (~1.1 ms на 103k) превышает шаг +
амортизированную перестройку (~0.4 ms). → будущая zero-copy-оптимизация оправдана на больших
N; на 15625 download всё ещё крупнейший bucket, но кадр sub-ms (ниже приоритет).

Оговорка по методике (анализ): аналитическая декомпозиция чистая — download меряется на УЖЕ
дренированной очереди, не маскирует незавершённый шаг/rebuild. Но в интегрированном профиле
приложения `syncFromGpuIfNeeded` может «забрать» ожидание предыдущих GPU-команд: такой
интегрированный bucket нельзя называть «чистым download» без pre-drain контроля. Transfer-
inclusive вариант render-upload (poll поверх пустого сабмита) исключён как шумный (cv >150% —
jitter планировщика, не реальный трансфер).

## Zero-copy рендер (per-frame download убран в чистом GPU-режиме)

Профиль выше показал крупнейший per-frame член GPU-кадра — блокирующее скачивание
позиций+скоростей GPU→CPU (downloadToCpu), которое делалось КАЖДЫЙ кадр, чтобы CPU-
потребители (рендер, связи, сетка, picking, метрики) видели свежие данные. Zero-copy
убирает его в типичном случае: рендер атомов в GPU-режиме биндит резидентные GPU pos/vel
напрямую (без скачивания и без CPU-упаковки этих атрибутов), а per-frame download стал
УСЛОВНЫМ — только когда активен реальный CPU-потребитель позиций (связи / сетка / выбран
атом / debug-панель / speed-color авто-нормировка). Действия вне рендера (picking/лассо/
удаление/добавление/сохранение) синкают on-demand в своих обработчиках.

| Сценарий (`BM_GpuDownloadCadence`, 30 кадров, N=64000) | Скачиваний за прогон |
|---|---|
| Чистый режим (атомы, обычная раскраска — нет CPU-потребителя) | **0** (было 30) |
| Активен CPU-потребитель (связи/сетка/...) | 30 (как раньше — данные свежи) |

Снятая стоимость: одно скачивание pos+vel @64k ≈ 350–420 μs (медиана 5; для 103k ≈
556–800 μs по профилю выше) убрано из КАЖДОГО чистого кадра. Так как download — крупнейший
per-frame член на большой сцене, в чистом GPU-режиме кадр заметно дешевле.

Провенанс: `Benchmarks/physics/BM_GpuDownloadCadence.cpp` (счётчик
`GpuResidentPhysics::downloadCount`, реальный `Simulation` в GPU-режиме); стоимость download —
`BM_GpuFrameBreakdown` (профиль выше); код — render-bind в `RendererWGPU::ensureAtomBindGroup`,
условный sync в `App/Application.cpp` (предикат cpuPositionConsumerActive, считается после
UI-апдейта кадра), on-demand sync в обработчиках тулов и сохранения. Картинка во всех режимах
та же; CPU-режим не тронут. Безопасность времени жизни буфера: резидентный pos/vel передаётся
рендеру по глобально-уникальному токену поколения (после выхода/входа в GPU-режим кэш
bind-group не переиспользует ссылку на освобождённый буфер; регресс-гейт `BM_GpuRenderBindApi`).

Оговорки:
- GPU резидентные числа — чистое вычисление, БЕЗ NL rebuild. CPU `ComputeForces`
  тоже без rebuild — сравнение apples-to-apples (верхняя граница). CPU FullStep
  с rebuild @ 103k ~16 ms.
- GPU full-step замер шумный (cv 53-119%) — launch jitter на батч-нагрузке.
- N=1000: GPU не выигрывает (launch overhead > вычисление); GPU полезен от ~10k.
- Корректность: GPU-траектория == CPU в пределах tolerance (`BM_GpuCorrectness`: на
  тест-сцене max|Δ|=0; гейт допускает 1e-2 — Full-GPU vs Half-CPU отличаются порядком
  суммирования, бит-в-бит не гарантирован).

Прочие оптимизации (не force-loop): bond hot path −50% (D1), формация бондов
75-121× O(N²)→O(N) (D2), рендер sparse-сетки 3.7× (D4) — детали в секциях ниже.

## Фаза 2.1 — soft-wall + гравитация на GPU (расширение за LJ-only)

Это НЕ оптимизация, а расширение функционала: резидентный GPU-шаг раньше считал
только LJ и МОЛЧА игнорировал мягкие стены и гравитацию. Теперь GPU считает их тоже,
поэтому GPU-траектория совпадает с CPU. Побочно чинит тихую дивергенцию: при
`gravity != 0` GPU-режим ронял гравитацию (атомы не падали). Планка корректности —
ПАРИТЕТ с CPU-владельцем `Engine/physics/ForceFields/WallForceField.cpp`.

Новый kernel `Rendering/shaders/physics_wall.wgsl` (`compute_wall`, per-atom, mobile-only)
диспатчится в `GpuResidentPhysics::step` между `zero_forces` и LJ — зеркалит CPU-порядок
`wall → pair` (`ForceField.cpp:136`). Зеркалит `WallForceField` точно: `k=500`,
`border=2`, `wallMax = worldSize − 1`, нижняя стена толкает в +, верхняя в −, гравитация
прибавляется как постоянная СИЛА (не ускорение), `forces.w` (PE) не трогается. Рантайм-смена
гравитации доставляется через `Simulation::setGravity` → бамп версии сцены → re-upload.

### Гейт паритета: `BM_GpuWallGravityParity`

| Кейс | Сцена | max\|Δ(CPU,GPU)\| | Порог | Примечание |
|---|---|---|---|---|
| static gravity | 9 атомов у нижней стены Y (penLow=0.6), gravity=(0,−5,0), массы H/Ar | **0.000e+00** (бит-в-бит) | 1e-2 | чистый паритет wall+gravity, без правок в hot loop |
| runtime gravity change | 9 атомов внутри box, g0=(0,−3,0) → g1=(2,4,0) после 20 шагов | **3.365e-03** | 1e-2 | одношаговый transient от re-upload (force-история зануляется); stale-gap ref = 1.883e-01 (≈56× → новая гравитация доставлена, не устарела) |
| регрессия LJ-only | `BM_GpuCorrectness` (gravity=0, атомы глубоко внутри) | **0** (без изменений) | 1e-2 | wall-kernel прибавляет ровно 0 → LJ-only паритет не сдвинулся |

Провенанс: формула — `WallForceField::applyWall/applyGravityForce` (`WallForceField.cpp:27-51`),
код GPU — `physics_wall.wgsl` + `GpuResidentPhysics.cpp:607-617` (диспатч), входы — сцены в
`BM_GpuWallGravityParity.cpp:68-98` (атомы в зоне `border`, смешанные массы H/Ar,
ненулевая гравитация). Смешанные массы важны: гравитация-как-сила даёт разные ускорения
(H ~1.008, Ar ~39.948), поэтому гейт ловит, что GPU копирует именно силу, а не «чинит» на
ускорение. Контракт uniform-буфера C++↔WGSL закреплён `static_assert(sizeof(WallUniforms)==48)`.

Воспроизведение:
```sh
./build/bench/benchmarks.exe --benchmark_filter='BM_GpuWallGravityParity|BM_GpuCorrectness'
```

## Фаза 2.2a — Morse-силы связей на GPU (статичная топология)

Расширение функционала: резидентный GPU-шаг раньше МОЛЧА игнорировал связи (в GPU-режиме
у сцены со связями не было ни Morse-, ни угловых сил, в отличие от CPU). Теперь GPU считает
**Morse pair-силы** статичных связей, паритетно CPU `Bond::forceBond`. Угловые силы — отдельная
под-стадия 2.2b. Топология **статична** в GPU-режиме (формация/разрыв заморожены — иначе нужен
per-step download→решение→upload round-trip, который убрал Шаг 2; динамическая формация вынесена
в 2.2c как отдельное исследование).

Реализация: новый kernel `Rendering/shaders/physics_bond.wgsl` (`compute_bond_morse`, per-atom
gather по резидентной bond-adjacency CSR `bondOffsets_`/`bondNeighbors_`/`bondParams_`, зеркаля
NL-структуру; каждая связь — два directed-ребра, как Full NL → Newton-3 без f32-атомиков).
Диспатч после LJ, перед correct (CPU-порядок `wall→LJ→bonds`). Топология заливается в `uploadBonds`
из `world.getBonds()`; `Simulation::addBond` бампит версию сцены (рантайм-связь долетает до VRAM).

Побочный фикс латентной дивергенции: GPU-шаг теперь чекает `isLJEnabled()` и пропускает диспатч
`compute_lj`, когда LJ выключен (раньше игнорировал — выключил LJ на CPU, перешёл в GPU → LJ молча
возвращался). `setLJEnabled` бампит версию → рантайм-смена долетает.

### Гейт паритета: `BM_GpuBondParity` (Morse-only)

| Проверка | Результат | Примечание |
|---|---|---|
| Morse-сила, GPU vs CPU | **0.000e+00** (бит-в-бит) | 3 изолированные пары степени-1 (C-C@1.1, O-H@0.9, C-C@1.1), off-equilibrium → Morse≠0; LJ/Кулон ВЫКЛ; смешанные массы O/C/H |
| CPU реально двинулся (не слеп) | self-disp **3.199e-02** | assert > 1e-4 до сверки: Morse активна |
| резидентный CSR == CPU-adjacency | точное совпадение | readback offsets/neighbors после `setGpuMode` + после рантайм-add |
| рантайм-add связи | edges **6→8** (+2) | `addBond` в GPU-режиме → re-upload → новые directed-рёбра |
| топология статична | bonds **3→3** | assert: CPU не порвал связь за прогон (иначе сцена невалидна) |
| рантайм setLJEnabled(false) долетает | parity **5.9e-05** vs stale-gap **1.84e-03** | GPU(LJ выкл) ≈31× ближе к CPU(LJ выкл), чем масштаб эффекта toggle → доставлено |
| регрессия LJ-only | `BM_GpuCorrectness` **max_abs=0** | пустой bond-CSR прибавляет ровно 0 |

Провенанс: формула — `Bond::MorseForce`/`forceBond` (`Bond.cpp:30-75`, dt отброшен, PE не пишется);
код GPU — `physics_bond.wgsl` + `GpuResidentPhysics.cpp` (диспатч после LJ); входы — сцена в
`BM_GpuBondParity.cpp`. Tolerance 1e-2; Morse-only бит-в-бит на осе-выровненных парах (CPU `MorseForce`
сам f32). Гейт прогоняется через реальный путь `Simulation::setGpuMode(true)`+`updateAll()`.

Воспроизведение:
```sh
./build/bench/benchmarks.exe --benchmark_filter='BM_GpuBondParity|BM_GpuCorrectness'
```

## Фаза 2.2b — Угловые силы связей на GPU (статичная топология)

Расширение функционала: GPU добавил **угловые силы** связей (гармонический потенциал вокруг
равновесного угла theta_0=60°, жёсткость k=50), паритетно CPU `Bond::angleForce`. Это финальная
силовая под-стадия связей; формация/разрыв (2.2c) вынесены отдельно. Топология по-прежнему
**статична** в GPU-режиме.

Реализация: новый entry `compute_bond_angle` в `Rendering/shaders/physics_bond.wgsl` —
двух-ролевой per-atom gather по той же резидентной bond-CSR (новых буферов не нужно). Атом i
накапливает СВОЮ угловую силу в двух ролях: роль A — i центр, перебор неупорядоченных пар (b,c)
своих рёбер → член `force_o` каждой тройки; роль B — i плечо, для каждого соседа-центра o перебор
ДРУГИХ соседей o (двух-хоповый обход CSR) → член `force_b` тройки (o, i, c). Сила на плечо зависит
только от (центр, своё плечо, другое плечо), а `force_scale` симметричен → единая `force_b`-форма
даёт верный CPU-член независимо от метки b/c (подтверждено численно, см. ниже). Каждый член каждой
тройки попадает ровно одному атому ровно раз → сумма == CPU `applyAngleForces`. theta_0/k берутся
из `bondUniform_` (== `Bond.cpp:116,121`). Отдельный 5-binding layout `bondAngleLayout_` (без
`bondParams` — угол берёт только геометрию). Диспатч `gTotal` после Morse, перед correct
(CPU-порядок `forceBond` → `applyAngleForces`).

### Гейт паритета: `BM_GpuBondParity` (angle sub-case)

| Проверка | Результат | Примечание |
|---|---|---|
| Угловая сила, GPU vs CPU | **2.861e-06** | триплет H-O-H (O центр, 2×O-H @r0≈0.957, угол ≈90° != 60°); LJ/Кулон ВЫКЛ; массы O(16)+H(1)+H(1); 50 шагов |
| CPU реально двинулся (не слеп) | self-disp **1.862e-01** | assert > 1e-4 до сверки; старт на r0 → Morse≈0, смещение несёт ИМЕННО угловой член |
| центр имеет угол | degree **2** | assert: O в резидентном CSR имеет 2 соседа (иначе триплет не возник бы) |
| топология статична | bonds **2→2** | assert: CPU не порвал связь за прогон |
| Morse-only НЕ регрессировал | **0.000e+00** | угловое ядро при degree-1 (нет пар рёбер / двух-хоповых троек) прибавляет ровно 0 |
| регрессия LJ-only | `BM_GpuCorrectness` **max_abs=0** | пустой bond-CSR → угол прибавляет 0 |

Провенанс: формула — `Bond::angleForce` (`Bond.cpp:77-144`; clamp cos → acos → sin из sqrt(1-cos²),
гарды len≤1e-12 и sin²<1e-12, theta_0=60°·π/180, k=50, force_b/force_c/force_o; dt не участвует,
PE не пишется); код GPU — `physics_bond.wgsl` (`compute_bond_angle`, `angle_triplet`) +
`GpuResidentPhysics.cpp` (диспатч после Morse); входы — H-O-H сцена в `BM_GpuBondParity.cpp`,
50 шагов, dt=0.01.

Tolerance **1e-5** (порог по измерению, ~3.5× запас над измеренным 2.861e-06 под run-to-run
f32-недетерминизм). Угол считается ШИРЕ Morse-only (тот бит-в-бит 0): CPU считает угол ВЕСЬ в
double + `acos` + `1/sin`, GPU всё в f32 → дрейф не бит-в-бит. Дизайн §4.4 закладывал worst-case
до ~1e-1; на практике дрейф НАМНОГО меньше, потому что угол H-O-H (90°) далёк от
sin≈0-сингулярности → `acos`/`1/sin` хорошо обусловлены. (Острый/тупой угол у sin≈0 дал бы шире
из-за усиления `1/sin`-членом, но гард sin²<1e-12 отсекает сингулярность одинаково на CPU и GPU.)
Гейт прогоняется через реальный путь `Simulation::setGpuMode(true)`+`updateAll()`.

Воспроизведение:
```sh
./build/bench/benchmarks.exe --benchmark_filter='BM_GpuBondParity|BM_GpuCorrectness'
```

## 2.3 — Coulomb pair-силы на GPU (charge-gated)

Coulomb перенесён на GPU как per-atom Full-NL gather по ТОМУ ЖЕ резидентному NL, что LJ (тот же
cutoff, тот же fixed-skip), с charge-gating: `compute_coulomb` (`physics_coulomb.wgsl`) зеркалит
`CoulombForceField::pairInteraction` (`CoulombForceField.h:16-48`). Сила:
`dr = pos(j)-pos(i)`, `qqScale = 140.399645·qa·qb`, `invR = 1.0/sqrt(d2)` (НЕ `inverseSqrt` — повтор
CPU `1.0f/std::sqrt`, ближе к биту), `forceScale = qqScale·invR/d2`, `force_i -= dr·forceScale`;
PE `+= 0.5·qqScale·invR`. Лейн `.w` ПРИБАВЛЯЕТСЯ (как LJ, `physics_lj.wgsl:84-85`) → после
`zero→wall→LJ→coulomb` `.w` = LJ_PE + Coulomb_PE. Диспатч `gMobile` после LJ, перед bonds
(CPU-порядок: LJ+Coulomb в одном pair-loop перед bonds, `ForceField.cpp:57-82,136-138`), под флагом
`coulombEnabled_` (зеркало `ljEnabled_`; `setCoulombEnabled` бампит `cpuSceneVersion`). Заряды
резидентны в `charges_` (заливаются в `uploadFromCpu` рядом с типами).

### Гейт паритета: `BM_GpuCoulombParity`

| Проверка | Результат | Примечание |
|---|---|---|
| Coulomb-only, GPU vs CPU | **9.537e-07** | пара притяжения (+1/−1) + пара отталкивания (+1/+1), старт 4.5 в cutoff(5); H; LJ ВЫКЛ; 50 шагов |
| Комбинированный LJ+Coulomb, GPU vs CPU | **0.000e+00** | те же заряды + LJ ВКЛ; силы складываются в один `forces[out]` |
| Комбинированный PE (.w), GPU vs CPU | gpu **−51.236** / cpu **−51.236** (absdiff **0.000e+00**) | ловит баг `.w` SET-вместо-ADD: при SET GPU `.w` нёс бы только Coulomb_PE (LJ_PE затёрт). Порог абсолютный `< 0.25·\|LJ_PE\|` = **7.424e-03** (измеренный LJ_PE-сигнал **−2.970e-02**) |
| CPU реально двинулся (не слеп) | combined self-disp **8.791e-01** | assert > 1e-4 до сверки: Coulomb-сила активна (заряды НЕнулевы) |
| no-regression uncharged→0 | **0.000e+00** | все заряды 0 + Coulomb ВКЛ → `chargeA==0` short-circuit → добавляет ровно 0 |
| charge доставлен в VRAM | точное совпадение | `readbackCharges` == CPU `atoms.charge(i)` |
| Coulomb-toggle долетел | parity **9.562e-03** << stale-gap **3.103e-01** | `setCoulombEnabled(false)` после `setGpuMode` долетает (бамп версии); `!(parity < 0.25·stale)` ловит и NaN |
| регрессия LJ-only | `BM_GpuCorrectness` **max_abs=0** | charge-0 → Coulomb прибавляет 0; `coulombEnabled=false` на этих callsite'ах |

Провенанс: формула — `CoulombForceField::pairInteraction` (`CoulombForceField.h:27-40`; всё во float,
`qqScale=k·qa·qb`, `invR=1/sqrt(d2)`, `forceScale=qqScale·invR/d2`, `PE += 0.5·qqScale·invR`, знак
`force -= dr·scale`, гарды `chargeB==0`/`d2<=1e-6`/cutoff/fixed-skip); код GPU —
`physics_coulomb.wgsl` (`compute_coulomb`) + `GpuResidentPhysics.cpp` (диспатч после LJ, `charges_`,
`coulombEnabled_`, PE-readback); входы — заряженная сцена в `BM_GpuCoulombParity.cpp` (2 пары
+1/−1 и +1/+1, dt=0.01, 50 шагов).

Tolerance траектории **1e-2** (порог по измерению; Coulomb весь во float на ОБЕИХ сторонах, GPU всё
в f32 → дрейф уровня LJ-паритета `BM_GpuCorrectness`, НЕ уровня angle — нет double-геометрии, только
`sqrt`-форма + порядок суммирования). Измеренные дрейфы НАМНОГО ниже порога: combined и
uncharged **0.000e+00** (бит-в-бит — LJ держит сцену, .w/.xyz складываются точно), coulomb-only
**9.537e-07** (старт 4.5 далёк от epsilon-сингулярности 1/r^3 → дрейф мал; старт 1.5 дал бы взрыв,
дизайн §6.7). PE-порог ПРИВЯЗАН к измеренному LJ_PE-сигналу — той величине, что SET-баг затёр бы:
после комбинированного прогона на ФИНАЛЬНЫХ позициях считаем LJ_PE отдельной CPU-симуляцией
(LJ ВКЛ / Coulomb ВЫКЛ, один `updateAll`) → `LJ_PE-signal` = **−2.970e-02**; assert абсолютный
`absdiff < 0.25·|LJ_PE|` = **7.424e-03**. Измеренный `absdiff` = **0.000e+00** (combined PE бит-в-бит)
→ запас ниже порога ≈∞, что прямо доказывает `.w` ADD: при SET `absdiff` ≈ |LJ_PE| = **2.970e-02** >
порога → провал. Относительный порог по ПОЛНОЙ PE здесь был бы СЛЕП: SET сдвигает полную PE лишь на
`|LJ_PE|/|PE|` ≈ 6e-4 (LJ_PE мал на фоне Coulomb_PE), ниже типичного rel-порога. Плюс blindness-guard
`|LJ_PE| > 5e-3`: если сигнала нет — гейт не может поймать SET и падает с требованием усилить сцену.
Гейт прогоняется через реальный путь `Simulation::setGpuMode(true)`+`updateAll()` (тестирует
доставку `charges_` И `coulombEnabled_` через `uploadSceneToGpu`→`uploadFromCpu`).

Воспроизведение:
```sh
cmake -S . -B build/bench   # новый physics_coulomb.wgsl: shader GLOB не CONFIGURE_DEPENDS
cmake --build build/bench --target latticelab_benchmarks
./build/bench/benchmarks.exe --benchmark_filter='BM_GpuCoulombParity|BM_GpuCorrectness'
```

## C1 — force loop фильтрует по физическому cutoff

`a72aff1 fix: pair-силы фильтруются по физическому cutoff, не listRadius`

Раньше force loop обрабатывал все пары до `listRadius = cutoff + skin`,
из-за чего изменение skin молча меняло физику. После C1 — фильтр
`d2 > cutoffSqr` во внутреннем цикле.

| N atoms | Before C1 | After C1 | Delta |
|---|---|---|---|
| 1000 | 42'561 ns | 31'107 ns | **-27%** |
| 15625 | 742'394 ns | 550'127 ns | **-26%** |
| 103823 | 5'120'338 ns | 3'733'707 ns | **-27%** |

Бенч: `SimulationFixture/ComputeForcesWithNeighborList`.

## D1 — scratch-буфера BondForceField живут между шагами

`722d0c4 perf: scratch-буфера BondForceField живут между шагами`

`applyAngleForces` каждый шаг аллоцировал `vector<uint16_t>(N)` и
`vector<vector<size_t>>(N)`. После D1 — `mutable`-члены с .clear()
между вызовами.

| N atoms | Before D1 | After D1 | Speedup |
|---|---|---|---|
| 125 | 6'098 ns | 2'672 ns | **2.3×** |
| 512 | 25'968 ns | 12'071 ns | **2.2×** |
| 4096 | 214'952 ns | 103'249 ns | **2.1×** |
| 8000 | 430'803 ns | 203'011 ns | **2.1×** |
| 15625 | 1'641'592 ns | 879'926 ns | **1.9×** |

Бенч: `BondedChainFixture/BondForcesCompute`.

## D2 — dup-check Bond::CreateBond через per-atom adjacency

`ae64056 perf: dup-check Bond::CreateBond через per-atom adjacency`

### Проблема

При включённой формации связей (`allowBondFormation=true`)
`BondForceField::formBonds` обходит **каждую** пару из NeighborList и для
каждой вызывает `tryCreateBond` → `Bond::CreateBond`. Внутри `CreateBond`
дубликат-проверка была линейным сканом всего списка связей:

```cpp
if (std::ranges::any_of(bonds, [&](const Bond& b) {
        return (b.aIndex == aIndex && b.bIndex == bIndex) ||
               (b.aIndex == bIndex && b.bIndex == aIndex);
    })) { return nullptr; }   // bonds — std::list<Bond>
```

`bonds` — это `std::list<Bond>`, и скан идёт по **всем** B существующим
связям. Обозначим C — число кандидатных пар за один `compute()` (растёт как
N × среднее число соседей), B — число уже созданных связей (растёт линейно
с N по мере формации). Тогда стоимость дубликат-проверок за вызов:

> C кандидатов × O(B) скан = **O(C · B) ≈ O(N²)**

Плюс `std::list` — это связный список с pointer-chasing: каждый узел в своём
месте кучи, скан кэш-недружелюбен, так что константа при O(B) ещё и большая.

Масштаб катастрофы виден в бенче (`BondFormationFixture/BondCompute`, решётка
углерода): один вызов `compute()` на 8000 атомов — **437 мс**, на 15625 —
**1.58 секунды**. Рост 8000→15625 (×1.95 по N) дал ×3.6 по времени — характерная
суперлинейность O(N²).

### Решение

Добавлен per-atom adjacency-индекс: для каждого атома — список индексов
атомов, с которыми он уже связан.

```cpp
using Adjacency = std::vector<std::vector<uint32_t>>;  // Bond.h
```

`Bond::CreateBond` получил опциональный параметр `Adjacency* adjacency`
(по умолчанию `nullptr` — тогда работает прежний линейный скан, для редких
одиночных вызовов из UI вроде `Simulation::addBond`). Когда индекс передан,
дубликат-проверка — это `std::find` в списке соседей **одного** атома:

```cpp
const auto& neighbors = (*adjacency)[aIndex];
if (std::find(neighbors.begin(), neighbors.end(), bIndex) != neighbors.end())
    return nullptr;                       // O(degree), не O(B)
// при успехе — обновляем индекс в обе стороны:
(*adjacency)[aIndex].push_back(bIndex);
(*adjacency)[bIndex].push_back(aIndex);
```

`BondForceField::compute` строит свежий adjacency из живых связей в начале
фазы формации (O(N + B)) и держит его как `mutable`-член (scratch,
переиспользуется между шагами — `.clear()` по элементам, без аллокаций),
затем передаёт в `formBonds` → `tryCreateBond` → `CreateBond`.

### Почему теперь O(N)

Степень атома ограничена его валентностью — у углерода ≤ 4 связей, у
большинства элементов ≤ 6-8. Значит per-atom список соседей крошечный и
почти константный, а дубликат-проверка из O(B) превращается в O(degree) ≈
O(1). Суммарная стоимость падает с O(C · B) до O(C · degree) ≈ O(C) ≈ O(N).
Построение индекса O(N + B) на шаг ничтожно на фоне устранённого O(N²).

### Числа

| N atoms | Before D2 | After D2 | Speedup |
|---|---|---|---|
| 125 | 33'706 ns | 30'410 ns | 1.1× |
| 512 | 211'936 ns | 168'693 ns | 1.3× |
| 4096 | 3'941'534 ns | 1'692'298 ns | **2.3×** |
| 8000 | 436'678'900 ns | 5'806'302 ns | **75×** |
| 15625 | 1'580'959'100 ns | 13'109'408 ns | **121×** |

Бенч: `BondFormationFixture/BondCompute` (`allowBondFormation=true`, решётка C
с шагом 1.7, близким к равновесию C-C). На малых N (125, 512) выигрыша почти
нет — B мало, O(B) скан дёшев. Эффект растёт с N: на 15625 устранение O(N²)
даёт **121×**. Совместимость: одиночные вызовы `addBond` без adjacency идут
прежним путём — защищено тестом `BondTest.CreateBondRejectsDuplicate`.

## D3 — render storage buffer 1.5× headroom

`82f477b perf: storage-буфера атомов растут с 1.5x запасом`

`ensureStorageBuffers` раньше пересоздавал все 5 storage-буферов
ровно под `count`. При интерактивном добавлении "+1 атом" — O(N)
recreate-ов. После D3 — `newCapacity = max(count, sbCapacity * 3/2 + 1)`,
O(log N) recreate-ов.

На существующих DrawShot-бенчах со статическим N эффект неизмеримый
(первый кадр растит до `count*1.5`, дальше fast path). Регресса нет.
Польза проявится в interactive scenarios с растущей популяцией.

## D4 — render сетки идёт только по непустым клеткам

`0767df9 perf: рендер сетки идёт только по непустым клеткам SpatialGrid`

`drawGridImpl` шёл тройным for-loop'ом по всем интерьерным клеткам
(O(cells.x · cells.y · cells.z)). На сцене 300³ с cellSize=5 это
~216k итераций на кадр. После D4 — `SpatialGrid::nonEmptyCells()`
возвращает список линейных индексов непустых клеток, собранный
в rebuild.

| N atoms | Before D4 | After D4 | Speedup |
|---|---|---|---|
| 125 | 707'545 ns | 221'092 ns | **3.2×** |
| 1000 | 1'464'263 ns | 374'347 ns | **3.9×** |
| 8000 | 2'099'109 ns | 561'943 ns | **3.7×** |

Бенч: `RendererFixture<Renderer3D>/DrawShotSparseGrid` (`drawGrid=true`).

## D5 — pair-force loop параллелится на TBB

`c30862a perf: pair-force loop параллелится на TBB через full NL и порог 5k`
`a5d549e perf: NeighborList сам выбирает Half или Full на каждом rebuild`

NL получил режимы Half / Full и auto-mode (выбор по `mobileCount`,
порог 5000). В Full force loop проходит по каждой паре дважды (один раз с
каждой стороны), но пишет только в собственный forceX → нет race, можно
`tbb::parallel_for`. На малых сценах автомат остаётся в Half.

| N atoms | Serial (Half NL) | D5 auto (Full + parallel) | Speedup |
|---|---|---|---|
| 1000 | 31'107 ns | 34'922 ns | ~ (auto Half) |
| 15625 | 550'127 ns | 200'932 ns | **2.7×** |
| 103823 | 3'733'707 ns | 955'105 ns | **3.9×** |

Бенч: `SimulationFixture/ComputeForcesWithNeighborList`. С C1 включённым
этот же кернел уже был на 27% быстрее по сравнению с pre-C1. Cumulative
для force loop @ 103k = 5.07ms (pre-C1) → 0.95ms (после C1+D5) = **5.3×**.

## Сводная таблица суммарных wins

Интегральные ускорения от upstream `fd28dc8` до HEAD:

| Hot path | Сцена | Pre-optim | Post-optim | Cumulative speedup |
|---|---|---|---|---|
| Pair force compute | N=103823 atoms | 5.07 ms | 0.95 ms | **5.3×** |
| Pair force compute | N=15625 atoms | 0.72 ms | 0.20 ms | **3.6×** |
| Bond force compute (chain) | N=8000 atoms | 0.43 ms | 0.20 ms | **2.1×** |
| Bond compute с formation | N=8000 atoms | 437 ms | 5.8 ms | **75×** |
| Bond compute с formation | N=15625 atoms | 1580 ms | 13 ms | **121×** |
| Render sparse grid | N=8000 atoms | 2.10 ms | 0.56 ms | **3.7×** |

## Correctness fixes (не perf, но важно)

- `a72aff1` C1: skin теперь не влияет на физический радиус сил
- `14f4dfe` C2 engine: NL отказывается работать при `cellSize < listRadius`
- `6cb3355` C2 UI: слайдеры NL clamp'ятся so что инвариант не нарушить
- `495ee9e` C3: RK4/Langevin в UI задизаблены пока они стабы Verlet
- `213b08f` C4: physics-accumulator работает как fixed-timestep с catch-up
- `eb9dddd` C5: `addAtom(fixed=true)` документировано как декоративный

## D6 GPU compute — de-risking findings

GPU LJ compute backend начат на ветке `gpu-compute` (Phase 1+2). Перед тем
как вкладывать ~2 недели в kernel-оптимизации (Morton reorder, shared-mem
tiling), был построен de-risking микробенч `BM_GpuLJReorder` (3 порядка
атомов: Identity / Random / Morton), измеряющий ЧИСТОЕ kernel-время через
batched dispatch (50 dispatch на submit + один poll, без per-call readback).

Результат (clang, i7-14700K, RTX 4070 SUPER, N=103823, repetitions=5, median,
per-dispatch = wall/50):

| Ordering | per-dispatch kernel | vs CPU pair (925 μs) |
|---|---|---|
| Identity (natural lattice) | ~83 μs | 11× быстрее CPU |
| Morton | ~108 μs | хуже Identity |
| Random | ~214 μs | 4× быстрее CPU |

Выводы, переворачивающие первоначальный roadmap:

1. **Kernel НЕ bottleneck.** Чистое kernel-время ~83 μs, не ~2 ms. Прежний
   замер `GpuPairForceFixture/Compute` = 3557 μs был доминирован
   readback+poll (~3400 μs), а не вычислением.
2. **Morton reorder не помогает** на регулярной решётке — natural insertion
   order уже spatially coherent; Morton (108 μs) медленнее Identity (83 μs).
3. **Kernel при любом порядке (83-214 μs) в 4-11× быстрее CPU pair (925 μs).**
   Locality значима (Random/Identity = 2.6×), но natural order её ловит, и
   даже worst-case Random остаётся быстрее CPU.

Следствие: kernel-оптимизации (reorder/tiling/tuning) гоняются за
не-bottleneck'ом. Весь выигрыш GPU-пути — в устранении per-step readback
(резидентный integrator на GPU), не в kernel'е. Roadmap сокращён с 4 стадий
до одной (resident integrator).

Воспроизведение:
```sh
./build/bench/benchmarks.exe --benchmark_filter=GpuLJReorder \
    --benchmark_min_time=0.3s --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true
```

### Resident GPU full-step — доказательство win

`BM_GpuFullStep` гоняет резидентный GPU-шаг (predict + confine + zero_forces +
LJ + correct, все на GPU через integrate_verlet.wgsl + physics_lj.wgsl) БЕЗ
per-step readback: 20 шагов в одном submit + один poll, per-step = wall/20.

Результат (RTX 4070 SUPER, repetitions=5, median):

| N | resident GPU step | CPU steady-state step (≈force+correct) | speedup |
|---|---|---|---|
| 15625 | ~25 μs | ~210 μs (200 force + 10 correct) | ~8× |
| 103823 | ~140 μs | ~1015 μs (955 force + 60 correct) | ~7.5× |

Это подтверждает вывод микробенча: убрав readback (бывшие ~3400 μs), получаем
~140 μs/шаг — kernel (~83 μs) + integrator-проходы. Kernel-оптимизации не
понадобились.

**Оговорки замера** (честность по measurement-validity из анализа):
- Steady-state БЕЗ NL rebuild и БЕЗ render sync. Реальная per-frame стоимость
  добавит периодический NL rebuild (positions нужны на CPU → download) и 60 Гц
  render (positions на CPU, пока нет zero-copy). Эти sync НЕ входят в 140 μs.
- Бенч измеряет только COST шага (5 dispatch); корректность prev/cur force swap
  не wired (для тайминга это не важно — kernel'ы те же).
- cv высокий (24-67%): мелкая нагрузка, launch overhead доминирует variance.
  Вывод (140 μs << 1 ms) робастен к шуму.

Воспроизведение:
```sh
./build/bench/benchmarks.exe --benchmark_filter=GpuFullStep \
    --benchmark_min_time=0.3s --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true
```

### Что осталось для production GPU-режима

Замер доказал steady-state win; для реального приложения нужна инженерная
интеграция (отдельный трек), с корректностью по блокерам из анализа:
- `GpuPhysicsState` owner резидентных буферов + `Simulation::setGpuMode`.
- Корректный prev/cur force swap (ping-pong bind groups, не copy) +
  correctness-гейт vs CPU Verlet (tolerance band).
- NL rebuild cadence: GPU max-displacement флаг (atomicMax-over-float
  невозможен в WGSL → integer/atomicOr threshold) или fixed cadence; download
  positions только на rebuild.
- Renderer zero-copy (positions+velocities из GpuPhysicsState; type/radius/
  selection остаются renderer-owned) либо явный per-frame sync.
- GPU mode считает LJ + soft-wall + gravity + силы связей Morse (2.2a) и угловые
  (2.2b) по СТАТИЧНОЙ топологии + Coulomb (2.3, charge-gated) (паритет с CPU
  `WallForceField`/`Bond::forceBond`/`Bond::angleForce`/`CoulombForceField` проверяют
  `BM_GpuWallGravityParity`/`BM_GpuBondParity`/`BM_GpuCoulombParity`). Остаётся off:
  образование/разрыв связей (2.2c — отдельное исследование).

### Kernel-оптимизации — отклонены

reorder/tiling/tuning отклонены de-risking замером (`BM_GpuLJReorder`): kernel
не bottleneck (83-214 μs при любом порядке, в 4-11× быстрее CPU pair).

### -ffast-math — измерено, отклонено (как perf-вариант)

Опция `FASTMATH_PHYSICS` (CMakeLists) добавляет `-ffast-math` ТОЛЬКО на `latticelab_lib`
(не на зависимости). Проверено head-to-head на одной машине (флаг подтверждён в
`compile_commands.json` на всех 73 файлах lib). **Дефолт OFF.**

Физику НЕ ломает:

| Проверка | Строгая (`-O3`) | `-ffast-math` |
|---|---|---|
| 36 unit-тестов | зелёные | зелёные |
| BaselineParity vs upstream fd28dc8 (траектория 20 шагов) | 2.094e-05 | 2.094e-05 |
| Силы в cutoff vs upstream | бит-в-бит | бит-в-бит |
| `BM_GpuCorrectness` (LJ Verlet, CPU vs GPU) | max_abs=0 | max_abs=0 |
| Morse (CPU vs GPU) | 0.000e+00 | 0.000e+00 |
| Angle | 2.861e-06 | 2.861e-06 |
| Coulomb (траектория / PE absdiff) | 0 / 0 | 4.77e-07 / 7.6e-06 |

Единственный измеримый эффект — сдвиг Кулона ~5e-7 (траектория) / ~8e-6 (PE) от
reassociation FP; много меньше порогов гейтов (1e-2 / 7.4e-3). LJ и Morse остаются
бит-в-бит даже с `-ffast-math`.

Но ускорения НЕ даёт (`SimulationFixture/ComputeForcesWithNeighborList`, real_time):

| N атомов | Строгая | `-ffast-math` |
|---|---|---|
| 15 625 | 253 μs | 274 μs |
| 103 823 | 1239 μs | 1304 μs |

В пределах шума, даже чуть медленнее. Причина: force-loop **memory-bound** (случайный
доступ к позициям соседей по NL) и уже векторизован на `-O3 -march=native` + TBB —
FP-throughput не узкое место, поэтому reassociation/rsqrt-аппроксимации `-ffast-math`
ничего не дают. GPU-ядра он не трогает (это WGSL). Вывод: безопасен, но бесполезен как
perf-вариант — опция оставлена выключенной и задокументирована.

Провенанс: процедура — реконфиг `build/bench` с `-DFASTMATH_PHYSICS=ON`, те же бенчи/тесты
back-to-back со строгой сборкой; машина — i7-14700K / RTX 4070 SUPER, clang Release.

---

# ПОСЛЕ РЕФАКТОРА — 3-библиотечная раскладка (коммиты после merge `7a641ccc`)

Всё **выше** этой черты снято **до** рефактора (старая плоская раскладка). Ниже — **после**,
переснято на новой раскладке.

## Пост-мерж — построение списка соседей параллелится на TBB

`e6b09332 perf: параллельное построение списка соседей на TBB (CPU full-step 3.2-4x)`

После слияния upstream горячий CPU-путь остался частично последовательным: pair-force loop уже
шёл на TBB (D5), но `NeighborList::build` (перестройка cell-list + списка) строился в один поток
и на больших сценах доминировал в полном шаге. Построение распараллелено по схеме
count → exclusive-scan offsets → write: каждый атом считает соседей (чтение сетки/позиций
read-only → нет гонок), затем по префиксной сумме пишет в свой непересекающийся срез. Порядок
соседей сохранён → результат побитово равен последовательному (`NeighborListTest.MatchesBruteForce`).
Порог 5000 атомов (ниже — последовательный путь, оверхед TBB не окупается).

Изоляция P1 — один и тот же `FullStepWithNeighborList`, BEFORE = замер ДО P1 (NL build
последовательный, pair force уже на TBB), AFTER = NL build тоже на TBB:

| N атомов | BEFORE (NL build послед.) | AFTER (NL build на TBB) | ускорение |
|---|---|---|---|
| 103 823 | ~23.4 ms | ~5.9 ms | ~4.0× |
| 1 000 000 | ~208 ms | ~56 ms | ~3.7× |

Полный шаг на трёх конфигурациях (i7-14700K / RTX 4070 SUPER, High-priority, медиана 5), шаг/с:

| N атомов | CPU 1 ядро | CPU все 28 ядер (TBB) | GPU резидентный |
|---|---|---|---|
| 103 823 | 56 (17.9 ms) | 228 (4.4 ms) | ~9 780 (0.102 ms) |
| 1 000 000 | 4.6 (216 ms) | 21 (47.9 ms) | ~962 (1.04 ms) |

CPU все ядра vs 1 ядро: ~4× (104k) / ~4.6× (1M); GPU vs CPU все ядра: ~43× (104k) / ~46× (1M).
1M в один поток шумный (cv ~20%, мало перестроек NL в окне) — число порядковое. Однопоток на
уровне чистого upstream fd28dc8 (104k 58, 1M ~4.4 шаг/с) — это итог после фикса двух регрессий
(ниже); первые пост-мерж замеры 1-ядра были в РАЗЫ хуже.

### Две однопоточные регрессии (найдены по фидбеку upstream, исправлены)

Первая пост-мерж версия имела однопоток в РАЗЫ хуже базы fd28dc8 (1 ядро: 104k ~19, 1M ~0.8
шаг/с против базы 58 / ~4.4). Обе причины — наши правки, не отсутствие upstream-оптимизаций
(Morton-сортировки у нас нет, но её нет и у fd28dc8, а он был быстрее):

1. Параллельная форма данных (Full NL = 2× парной работы + 3-проходный parallel-build)
   выбиралась по числу атомов, без учёта реального параллелизма — на 1-2 ядрах чистый штраф.
   Фикс: гейт по эффективному TBB-параллелизму (минимум global_control и ёмкости арены; НЕ
   hardware_concurrency — он не видит ограничения потоков). < 4 ядер → Half NL + serial build.

2. Фильтр физического cutoff (C1) стоял веткой `if (d2 > cutoffSqr) continue;` в горячем
   pair-force цикле. На плотной сцене список доходит до listRadius = cutoff + skin (~32 соседа
   против ~18 в cutoff), и data-dependent переход дороже экономии — цикл сил был ~2× медленнее.
   Фикс: cutoff как множитель active = (d2 ≤ cutoffSqr) ? 1 : 0 в LJ/Coulomb pairInteraction —
   за cutoff вклад × 0, бит-в-бит как пропуск, но без перехода. Плюс шаблон по режиму NL: Half
   не платит за fixed-проверку соседа, writeNeighbor стал compile-time.

После обоих фиксов 1 ядро вернулось к базе (104k 56 vs 58; 1M 4.6 vs 4.4 шаг/с, в пределах
шума). Многоядерный путь регресс не затрагивал — на всех 28 ядрах до и после фикса почти одно
и то же (1M ~49 → 48 ms/шаг, 104k ~5.2 → 4.4 ms/шаг; cutoff-фикс дал небольшой бонус на 104k,
где доля force-loop выше). Багом просел только однопоток (1M ~1250 → 216 ms/шаг). Физика
бит-в-бит, 36/36 юнит-тестов.

Провенанс: `FullStepWithNeighborList` (`Lattice/benchmarks/physics/BM_SimulationStep.cpp`);
1 ядро — env `LL_TBB_THREADS=1` (global_control; НЕ affinity-pin — pin сгоняет все воркеры на
одно ядро и даёт ложный спин-трэшинг, завышавший штраф); процесс High-priority (иначе 1M cv
высок); GPU — `BM_GpuFullStep_Resident` (per-iter / 20 шагов); код фиксов — `NeighborList.cpp`
(гейт concurrency), `ForceField.cpp` + `LJForceField.h` + `CoulombForceField.h` (branchless
cutoff + Half/Full шаблон); сравнение с базой — worktree fd28dc8, тот же бенч, ucrt64 clang,
флаги -O3 -march=native; корректность — `NeighborListTest.MatchesBruteForce` +
`BaselineParityTest`, 36/36. Сборка `cmake --preset bench`.

## Воспроизведение

```sh
cmake --preset bench
cmake --build --preset bench
./build/bench/benchmarks.exe \
    --benchmark_format=json \
    --benchmark_out=results.json \
    --benchmark_repetitions=3 \
    --benchmark_min_time=0.3s \
    --benchmark_report_aggregates_only=true
```

Для теста корректности:

```sh
cmake --preset tests
cmake --build --preset tests
./build/tests/latticelab_tests.exe
```
