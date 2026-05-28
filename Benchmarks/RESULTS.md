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

`Bond::CreateBond` делал линейный скан `std::list<Bond>` для каждой
кандидатной пары при формации бондов. O(B) на dup-check × C кандидатов
= O(B·C). После D2 — `vector<vector<uint32_t>>` per-atom adjacency,
O(degree) lookup.

| N atoms | Before D2 | After D2 | Speedup |
|---|---|---|---|
| 125 | 33'706 ns | 30'410 ns | 1.1× |
| 512 | 211'936 ns | 168'693 ns | 1.3× |
| 4096 | 3'941'534 ns | 1'692'298 ns | **2.3×** |
| 8000 | 436'678'900 ns | 5'806'302 ns | **75×** |
| 15625 | 1'580'959'100 ns | 13'109'408 ns | **121×** |

Бенч: `BondFormationFixture/BondCompute` (`allowBondFormation=true`).
На больших N это устранение O(N²) поведения.

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

## Что не сделано

- **D6 GPU compute**: порт LJ-ядра на WGSL compute shader. Потенциал
  10-50× на N>50k, но multi-hour работа и архитектурное изменение —
  отдельный трек.

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
