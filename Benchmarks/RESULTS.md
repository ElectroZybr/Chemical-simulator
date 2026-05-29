# Benchmark results

Зафиксированные числа выигрышей по сериям коммитов на ветке
`optimization-research` поверх upstream `fd28dc8`.

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

Оговорки:
- GPU резидентные числа — чистое вычисление, БЕЗ NL rebuild. CPU `ComputeForces`
  тоже без rebuild — сравнение apples-to-apples (верхняя граница). CPU FullStep
  с rebuild @ 103k ~16 ms.
- GPU full-step замер шумный (cv 53-119%) — launch jitter на батч-нагрузке.
- N=1000: GPU не выигрывает (launch overhead > вычисление); GPU полезен от ~10k.
- Корректность: GPU-траектория бит-в-бит == CPU (`BM_GpuCorrectness`).

Прочие оптимизации (не force-loop): bond hot path −50% (D1), формация бондов
75-121× O(N²)→O(N) (D2), рендер sparse-сетки 3.7× (D4) — детали в секциях ниже.

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
- LJ-only ограничение (wall/bond/Coulomb off в GPU mode) — bond/wall на GPU
  это дальнейший трек.

### Kernel-оптимизации — отклонены

reorder/tiling/tuning отклонены de-risking замером (`BM_GpuLJReorder`): kernel
не bottleneck (83-214 μs при любом порядке, в 4-11× быстрее CPU pair).

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
