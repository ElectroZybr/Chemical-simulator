// GPU counting-sort cell-list: строит тот же CSR `atomsInCells`, что и
// SpatialGrid::rebuild на CPU, но целиком на GPU из буфера позиций.
//
// Пайплайн (оркестрируется хостом, без cross-workgroup spin-wait):
//   clear_counts -> count_cells (atomicAdd по клетке)
//     -> exclusive scan cellCounts -> cellOffsets (иерархический, см. ниже)
//     -> copy_offsets_to_cursors -> scatter_atoms (atomic scatter в atomsInCells)
//
// Маппинг клетки ОБЯЗАН совпадать с CPU бит-в-бит, иначе GPU биннит иначе:
//   CPU toCell: int(coord/cellSize)+1, clamp в [1, max(1,size-2)] (SpatialGrid.h:79)
//   CPU index:  (z*size.y + y)*size.x + x                          (SpatialGrid.h:49)
// i32() в WGSL усекает к нулю так же, как static_cast<int> в C++ — тождественно и
// для отрицательных координат (i32(-0.5)==0), где затем clamp одинаково подтянет
// их в [1,hi] с обеих сторон (boundary-сцена гейта проверяет это явно).
//
// Биндинги объявлены в ОДНОМ модуле с уникальными слотами, НО хост биндит их
// РАЗДЕЛЬНЫМИ per-kernel bind group layout'ами: max_storage_buffers_per_shader_stage
// == 8, а модуль объявляет 9 storage-буферов, поэтому каждое ядро получает layout
// только со своими биндингами (≤8). Один физический буфер может биндиться и как
// atomic<u32>, и как plain u32 в разных проходах — атомарность это свойство
// ДОСТУПА в шейдере, не буфера (storage-буфер это просто память).
//
// Все атомарные операции над u32 (во всём проекте нет float-атомиков, только
// u32/i32 — см. nl_displacement.wgsl).

// n — двойного назначения: cellCount для grid-проходов и длина текущего уровня
// scan. Хост пишет нужное n перед каждым диспатчем.
struct Params {
    sizeX: u32,
    sizeY: u32,
    sizeZ: u32,
    atomCount: u32,
    cellSize: f32,
    n: u32,            // элементов на этом проходе (cellCount или длина уровня scan)
    listRadiusSqr: f32, // r_list^2 для NL-фильтра (бывший _pad0; 2b)
    cellCount: u32,     // всего клеток сетки (бывший _pad1; нужен NL-обходу для end-границы CSR клетки)
};

@group(0) @binding(0) var<uniform> u: Params;
@group(0) @binding(1) var<storage, read> positions: array<vec4<f32>>;
@group(0) @binding(2) var<storage, read_write> cellCounts: array<atomic<u32>>;
@group(0) @binding(3) var<storage, read> scanIn: array<u32>;
@group(0) @binding(4) var<storage, read_write> scanOut: array<u32>;
@group(0) @binding(5) var<storage, read_write> blockSums: array<u32>;
@group(0) @binding(6) var<storage, read> cellOffsets: array<u32>;
@group(0) @binding(7) var<storage, read_write> cellCursors: array<atomic<u32>>;
@group(0) @binding(8) var<storage, read_write> atomCells: array<u32>;
@group(0) @binding(9) var<storage, read_write> atomsInCells: array<u32>;

// ---- Шаг 2b: shadow-буфера GPU Full NeighborList (CSR) ----
// Маппинг клетки и центр атома берём из 2a: центр атома i — это atomCells[i]
// (ровно cellIndices_[i] на CPU), CSR клеток — cellOffsets[c]. Эти же буфера
// (binding 6/8/9) читаются NL-проходами как plain read.
@group(0) @binding(10) var<storage, read_write> neighborCounts: array<u32>; // соседей у атома i
@group(0) @binding(11) var<storage, read> nlOffsets: array<u32>;            // exclusive scan (длина atomCount+1; [i+1] = total после i)
@group(0) @binding(12) var<storage, read_write> nlNeighbors: array<u32>;     // плоские индексы соседей

const SCAN_THREADS: u32 = 256u;
const SCAN_BLOCK: u32 = 512u; // 2 элемента на поток (work-efficient Blelloch)

// ---- clear_counts: обнуляет cellCounts перед count ----
@compute @workgroup_size(256)
fn clear_counts(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.n) { return; }
    atomicStore(&cellCounts[i], 0u);
}

// Маппинг координаты в индекс клетки по одной оси — точная копия CPU toCell.
fn to_cell(coord: f32, cellSize: f32, axisSize: u32) -> i32 {
    let c: i32 = i32(coord / cellSize) + 1;
    let hi: i32 = max(1, i32(axisSize) - 2);
    return clamp(c, 1, hi);
}

fn cell_index(p: vec3<f32>) -> u32 {
    let cx: i32 = to_cell(p.x, u.cellSize, u.sizeX);
    let cy: i32 = to_cell(p.y, u.cellSize, u.sizeY);
    let cz: i32 = to_cell(p.z, u.cellSize, u.sizeZ);
    // index = (z*size.y + y)*size.x + x
    let idx: i32 = (cz * i32(u.sizeY) + cy) * i32(u.sizeX) + cx;
    return u32(idx);
}

// ---- count_cells: per-atom cell index + atomicAdd ----
@compute @workgroup_size(256)
fn count_cells(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.atomCount) { return; }
    let cell: u32 = cell_index(positions[i].xyz);
    atomCells[i] = cell;
    atomicAdd(&cellCounts[cell], 1u);
}

// ---- scan_block: блочный exclusive scan (Blelloch) ----
//
// Один workgroup сканирует SCAN_BLOCK элементов scanIn[base..base+SCAN_BLOCK),
// пишет exclusive-результат в scanOut и суммарный итог блока в blockSums[wid].
// Хвостовой блок (n не кратно SCAN_BLOCK) добивается нулями.
//
// Не single-workgroup и без spin-wait: число блоков на уровне может быть >1,
// уровни разруливаются ОТДЕЛЬНЫМИ диспатчами на стороне хоста (см. C++ driver).
var<workgroup> temp: array<u32, SCAN_BLOCK>;

@compute @workgroup_size(SCAN_THREADS)
fn scan_block(@builtin(local_invocation_id) lid: vec3<u32>,
              @builtin(workgroup_id) wid: vec3<u32>) {
    let tid: u32 = lid.x;
    let base: u32 = wid.x * SCAN_BLOCK;
    let n: u32 = u.n;

    let a0: u32 = base + 2u * tid;
    let a1: u32 = base + 2u * tid + 1u;
    var v0: u32 = 0u;
    var v1: u32 = 0u;
    if (a0 < n) { v0 = scanIn[a0]; }
    if (a1 < n) { v1 = scanIn[a1]; }
    temp[2u * tid] = v0;
    temp[2u * tid + 1u] = v1;

    // up-sweep (reduce)
    var offset: u32 = 1u;
    var d: u32 = SCAN_BLOCK >> 1u;
    loop {
        if (d == 0u) { break; }
        workgroupBarrier();
        if (tid < d) {
            let ai: u32 = offset * (2u * tid + 1u) - 1u;
            let bi: u32 = offset * (2u * tid + 2u) - 1u;
            temp[bi] = temp[bi] + temp[ai];
        }
        offset = offset * 2u;
        d = d >> 1u;
    }

    // Сумма всего блока -> blockSums, обнуляем последний для down-sweep.
    workgroupBarrier();
    if (tid == 0u) {
        blockSums[wid.x] = temp[SCAN_BLOCK - 1u];
        temp[SCAN_BLOCK - 1u] = 0u;
    }

    // down-sweep
    d = 1u;
    loop {
        if (d >= SCAN_BLOCK) { break; }
        offset = offset >> 1u;
        workgroupBarrier();
        if (tid < d) {
            let ai: u32 = offset * (2u * tid + 1u) - 1u;
            let bi: u32 = offset * (2u * tid + 2u) - 1u;
            let t: u32 = temp[ai];
            temp[ai] = temp[bi];
            temp[bi] = temp[bi] + t;
        }
        d = d * 2u;
    }
    workgroupBarrier();

    if (a0 < n) { scanOut[a0] = temp[2u * tid]; }
    if (a1 < n) { scanOut[a1] = temp[2u * tid + 1u]; }
}

// ---- add_block_offsets: прибавляет просканированный префикс блока ----
// scanIn здесь = просканированные blockSums (по одному на SCAN_BLOCK-блок).
// scanOut здесь = тот же буфер, что писал scan_block (in-place дописываем).
@compute @workgroup_size(SCAN_THREADS)
fn add_block_offsets(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.n) { return; }
    let block: u32 = i / SCAN_BLOCK;
    scanOut[i] = scanOut[i] + scanIn[block];
}

// ---- copy_offsets_to_cursors: cursors := offsets ----
@compute @workgroup_size(256)
fn copy_offsets_to_cursors(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.n) { return; }
    atomicStore(&cellCursors[i], cellOffsets[i]);
}

// ---- scatter_atoms: atomic scatter в atomsInCells ----
// atomicAdd(cursor[cell]) даёт слот. Порядок внутри клетки недетерминирован
// (atomic order) — gate сравнивает per-cell как МНОЖЕСТВО, не по порядку.
@compute @workgroup_size(256)
fn scatter_atoms(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.atomCount) { return; }
    let cell: u32 = atomCells[i];
    let slot: u32 = atomicAdd(&cellCursors[cell], 1u);
    atomsInCells[slot] = i;
}

// ---- Шаг 2b: GPU Full NeighborList build ----
//
// Зеркалит NeighborList::writeAtomNeighbors (Full mode): для атома i обходим 27
// клеток-соседей (центр = atomCells[i], тот же, что cellIndices_[i] на CPU),
// для каждого j!=i фильтр dx*dx+dy*dy+dz*dz <= listRadiusSqr. Half-обрыв (j>=i)
// НЕ применяется — resident GPU работает в Full.
//
// Границы CSR клетки c: begin=cellOffsets[c], end=cellOffsets[c+1] (для последней
// клетки cellOffsets[cellCount] отсутствует → end=atomCount, т.к. scan
// монотонен и off[last]+cnt[last]==atomCount). cellCounts (atomic) не трогаем —
// плотность клетки берём из разности offsets, чтобы не читать atomic как plain.
//
// Стенсил совпадает с SpatialGrid::rebuildNeighborOffsets бит-в-бит:
//   offset = dx + (dy + dz*sizeY)*sizeX,  dz,dy,dx in [-1,1]
// и складывается с центром так же, как CPU center+offsets27[k]. i32-арифметика
// центра+оффсета не уходит за [0,cellCount) благодаря ghost-слою (центр всегда в
// интерьере [1,size-2] по каждой оси — ровно как на CPU, где обход не bounds-checked).

// Конец CSR-слайса клетки c (см. коммент выше). c заведомо валиден (< cellCount).
fn cell_end(c: u32) -> u32 {
    if (c + 1u < u.cellCount) {
        return cellOffsets[c + 1u];
    }
    return u.atomCount;
}

// ВНИМАНИЕ: count_full и write_full ОБЯЗАНЫ обходить пары идентично (тот же
// стенсил, тот же порядок, тот же фильтр). Иначе курсор write выйдет за слайс
// [nlOffsets[i],nlOffsets[i+1]) и затрёт соседний атом. Логика НАМЕРЕННО
// продублирована, не вынесена в общую функцию: WGSL засчитывает статическое
// использование биндинга по достижимым функциям независимо от рантайм-флага, и
// общая функция тянула бы nlOffsets/nlNeighbors (11/12) в pipeline-layout count'а
// (max_storage_buffers и просто несоответствие layout'а). Любая правка фильтра/
// обхода — СРАЗУ в обе функции.

// count_full: число соседей атома i (Full). Трогает только positions/cellOffsets/
// atomCells/atomsInCells (биндинги 1/6/8/9) — НЕ nlOffsets/nlNeighbors.
fn count_full(i: u32) -> u32 {
    let pi: vec3<f32> = positions[i].xyz;
    let center: i32 = i32(atomCells[i]);
    let sx: i32 = i32(u.sizeX);
    let sy: i32 = i32(u.sizeY);
    var cnt: u32 = 0u;
    for (var dz: i32 = -1; dz <= 1; dz = dz + 1) {
        for (var dy: i32 = -1; dy <= 1; dy = dy + 1) {
            for (var dx: i32 = -1; dx <= 1; dx = dx + 1) {
                let cell: u32 = u32(center + dx + (dy + dz * sy) * sx);
                let begin: u32 = cellOffsets[cell];
                let end: u32 = cell_end(cell);
                for (var s: u32 = begin; s < end; s = s + 1u) {
                    let j: u32 = atomsInCells[s];
                    if (j == i) { continue; }
                    let pj: vec3<f32> = positions[j].xyz;
                    let ddx: f32 = pj.x - pi.x;
                    let ddy: f32 = pj.y - pi.y;
                    let ddz: f32 = pj.z - pi.z;
                    // Тот же явный фильтр, что CPU (NeighborList.h:75) — fp32 идентично.
                    if (ddx * ddx + ddy * ddy + ddz * ddz <= u.listRadiusSqr) {
                        cnt = cnt + 1u;
                    }
                }
            }
        }
    }
    return cnt;
}

// write_full: пишет соседей i в слайс [nlOffsets[i], nlOffsets[i+1]) по локальному
// курсору. Трогает positions/cellOffsets/atomCells/atomsInCells + nlOffsets(11)/
// nlNeighbors(12). Обход идентичен count_full.
fn write_full(i: u32) {
    let pi: vec3<f32> = positions[i].xyz;
    let center: i32 = i32(atomCells[i]);
    let sx: i32 = i32(u.sizeX);
    let sy: i32 = i32(u.sizeY);
    let base: u32 = nlOffsets[i];
    var cursor: u32 = 0u;
    for (var dz: i32 = -1; dz <= 1; dz = dz + 1) {
        for (var dy: i32 = -1; dy <= 1; dy = dy + 1) {
            for (var dx: i32 = -1; dx <= 1; dx = dx + 1) {
                let cell: u32 = u32(center + dx + (dy + dz * sy) * sx);
                let begin: u32 = cellOffsets[cell];
                let end: u32 = cell_end(cell);
                for (var s: u32 = begin; s < end; s = s + 1u) {
                    let j: u32 = atomsInCells[s];
                    if (j == i) { continue; }
                    let pj: vec3<f32> = positions[j].xyz;
                    let ddx: f32 = pj.x - pi.x;
                    let ddy: f32 = pj.y - pi.y;
                    let ddz: f32 = pj.z - pi.z;
                    if (ddx * ddx + ddy * ddy + ddz * ddz <= u.listRadiusSqr) {
                        nlNeighbors[base + cursor] = j;
                        cursor = cursor + 1u;
                    }
                }
            }
        }
    }
}

// ---- count_neighbors_full: neighborCounts[i] = |соседей i| ----
@compute @workgroup_size(256)
fn count_neighbors_full(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.atomCount) { return; }
    neighborCounts[i] = count_full(i);
}

// ---- write_neighbors_full: пишет соседей i (без атомиков, по слайсам nlOffsets) ----
@compute @workgroup_size(256)
fn write_neighbors_full(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.atomCount) { return; }
    write_full(i);
}
