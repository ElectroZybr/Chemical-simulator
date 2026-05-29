// Bond force compute kernels (per-atom gather over bond-adjacency CSR).
//
// Зеркалит CPU-владельца bond-сил (BondForceField::compute → Bond::forceBond,
// Bond.cpp:30-58, и в 2.2b — Bond::angleForce). Топология связей резидентна в
// VRAM как CSR (offsets+neighbors), зеркаля nlOffsets_/nlNeighbors_; каждая связь
// (a,b) хранится как ДВА directed edge a→b и b→a, чтобы каждый атом видел все свои
// рёбра при per-atom gather'е (как Full NL для LJ).
//
// Per-atom gather (паттерн LJ, physics_lj.wgsl:6-8): каждый thread обрабатывает
// одного атома i, СУММИРУЕТ вклад от его рёбер и пишет ТОЛЬКО свой forces[i] —
// race-free без f32-атомиков. Newton-3 держится глобально за счёт directed-
// дублирования рёбер (i получает +dir по ребру к j, j получает −dir по ребру к i).
//
// Диспатч по gTotal (НЕ gMobile, в отличие от wall/LJ): CPU Bond::forceBond пишет
// силу по индексам концов БЕЗ mobile/fixed-фильтра (Bond.cpp:51-57). Сила в
// fixed-слот безвредна (correct ходит только < mobileCount и её не двигает), но
// gTotal даёт точную семантику CPU-суммы и гарантирует, что mobile-сосед fixed-
// атома видит консистентную картину. Поэтому здесь НЕ фильтруем сосед по mobile.
//
// Контракт буферов (отдельный layout, см. GpuResidentPhysics):
//   - uniforms (binding 0): totalCount (+ резерв под theta_0/k_angle для 2.2b)
//   - positions (binding 1): read-only AoS vec4<f32> (x,y,z,pad), как LJ binding 1
//   - bondOffsets (binding 2): read-only u32, длина totalCount+1, CSR-стиль
//   - bondNeighbors (binding 3): read-only u32, длина 2*bondCount (directed edges)
//   - bondParams (binding 4): read-only vec4<f32> (r0, De, a, _) на directed edge,
//     параллельно bondNeighbors (bondParams[p] соответствует bondNeighbors[p]);
//     CPU фиксирует params при создании bond'а (Bond.h:40), kernel НЕ лезет в
//     type-таблицу.
//   - forces (binding 5): read_write vec4<f32> (fx,fy,fz,pe), как LJ binding 6.
//     Должен быть обнулён до dispatch (zero_forces); kernel прибавляет к .xyz.
//     Лейн .w (PE) НЕ трогаем: bonds энергию не пишут (Bond.cpp без energy()) —
//     энергетический канал остаётся LJ-only, паритетно с CPU.

struct BondUniforms {
    totalCount: u32,
    thetaZero: f32,  // == Bond.cpp:116 (60° в радианах) — резерв под angle (2.2b)
    kAngle: f32,     // == Bond.cpp:121 (50.0) — резерв под angle (2.2b)
    _pad0: u32,      // выравнивание uniform-структуры до кратного 16 байт
};

@group(0) @binding(0) var<uniform> u: BondUniforms;
@group(0) @binding(1) var<storage, read> positions: array<vec4<f32>>;
@group(0) @binding(2) var<storage, read> bondOffsets: array<u32>;
@group(0) @binding(3) var<storage, read> bondNeighbors: array<u32>;
@group(0) @binding(4) var<storage, read> bondParams: array<vec4<f32>>;
@group(0) @binding(5) var<storage, read_write> forces: array<vec4<f32>>;

// Morse pair-сила (зеркалит Bond::forceBond + Bond::MorseForce, Bond.cpp:30-58,72-75).
// dt ИГНОРИРУЕТСЯ на CPU ((void)dt, Bond.cpp:31) — никакого dt-члена тут нет.
@compute @workgroup_size(64)
fn compute_bond_morse(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.totalCount) {
        return;
    }

    let pi: vec3<f32> = positions[i].xyz;
    let begin: u32 = bondOffsets[i];
    let end: u32 = bondOffsets[i + 1u];

    var force: vec3<f32> = vec3<f32>(0.0, 0.0, 0.0);

    // Для атома i как «a» по отношению к соседу j=«b»: d = pos(a) - pos(b),
    // вклад в i = +d/|d|*Morse (Bond.cpp:37-57). По ребру к j (i=b, j=a) знак
    // инвертируется самим d = pi - pj → получаем CPU-вклад f[b] = -dir.
    for (var p: u32 = begin; p < end; p = p + 1u) {
        let j: u32 = bondNeighbors[p];
        let d: vec3<f32> = pi - positions[j].xyz;
        let dist: f32 = length(d);
        // Гард dist <= 1e-12 → 0 (Bond.cpp:41-43).
        if (dist > 1e-12) {
            let prm: vec4<f32> = bondParams[p]; // (r0, De, a, _)
            let expA: f32 = exp(-prm.z * (dist - prm.x));
            let mag: f32 = 2.0 * prm.y * prm.z * (expA * expA - expA);
            force = force + (d / dist) * mag;
        }
    }

    // Прибавляем к существующей силе (zero→wall→LJ уже отработали). .w (PE) не
    // трогаем — bonds энергию не пишут.
    let prev: vec4<f32> = forces[i];
    forces[i] = vec4<f32>(prev.xyz + force, prev.w);
}
