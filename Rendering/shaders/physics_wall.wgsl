// Soft-wall + gravity force kernel (per-atom, mobile-only).
//
// Зеркалит CPU-владельца WallForceField::compute (WallForceField.cpp:6-25):
// для каждого МОБИЛЬНОГО атома добавляет в его силу две контрибуции —
//   1) мягкую стену у граней бокса (applyWall, WallForceField.cpp:27-38);
//   2) постоянную силу gravity (applyGravityForce, WallForceField.cpp:47-51).
//
// Per-atom: считает силу атома только из его собственной позиции, без чтения
// соседей (в отличие от LJ). Аккумулятивен: прибавляет к forces[i].xyz (на
// момент dispatch = 0 после zero_forces, т.к. идёт ДО LJ — зеркалит CPU-порядок
// wall→LJ, ForceField.cpp:136-137). Лейн .w (PE) НЕ трогает: CPU тоже не пишет
// энергию в WallForceField, энергетический канал остаётся LJ-only — паритетно.
//
// Контракт буферов (отдельный layout, см. GpuResidentPhysics):
//   - uniforms (binding 0): worldMax, gravity, k, border, mobileCount
//   - positions (binding 1): read-only AoS vec4<f32> (x,y,z,pad), как LJ binding 1
//   - forces (binding 2): read_write vec4<f32> (fx,fy,fz,pe), как LJ binding 6.
//     Должен быть обнулён до dispatch (zero_forces); kernel прибавляет.

struct WallUniforms {
    worldMaxX: f32,   // worldSize - 1, == WallForceField.cpp:9 (wallMax) и StepOps.h:22
    worldMaxY: f32,
    worldMaxZ: f32,
    gravityX: f32,    // world.getGravity(), постоянная СИЛА (не ускорение, WallForceField.cpp:47-51)
    gravityY: f32,
    gravityZ: f32,
    k: f32,           // == WallForceField.cpp:28 (500.0)
    border: f32,      // == WallForceField.cpp:29 (2.0)
    mobileCount: u32,
    _pad0: u32,       // выравнивание uniform-структуры до кратного 16 байт
    _pad1: u32,
    _pad2: u32,
};

@group(0) @binding(0) var<uniform> u: WallUniforms;
@group(0) @binding(1) var<storage, read> positions: array<vec4<f32>>;
@group(0) @binding(2) var<storage, read_write> forces: array<vec4<f32>>;

// Одна ось мягкой стены (зеркалит WallForceField::applyWall, WallForceField.cpp:27-38).
// Нижняя стена толкает в +, верхняя в −; вне зоны border обе нулевые.
// pen^6 считаем как p2*p2*p2 (p2=pen*pen) — повторяет порядок умножений CPU
// ближе к биту, чем pow(pen, 6.0) (важно для паритет-гейта, design §2.1 Q4).
fn wall_axis(coord: f32, maxv: f32, k: f32, border: f32) -> f32 {
    let penLow: f32 = border - coord;
    let penHigh: f32 = coord - (maxv - border);

    var fLow: f32 = 0.0;
    if (penLow > 0.0) {
        let p2: f32 = penLow * penLow;
        fLow = p2 * p2 * p2 * k;
    }
    var fHigh: f32 = 0.0;
    if (penHigh > 0.0) {
        let p2: f32 = penHigh * penHigh;
        fHigh = p2 * p2 * p2 * k;
    }
    return fLow - fHigh;
}

@compute @workgroup_size(64)
fn compute_wall(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    // Только mobile (WallForceField.cpp:11: цикл < mobileCount). Fixed-атомы
    // (>= mobileCount) wall/gravity не получают — как на CPU.
    if (i >= u.mobileCount) {
        return;
    }

    let p: vec3<f32> = positions[i].xyz;
    let fw: vec3<f32> = vec3<f32>(
        wall_axis(p.x, u.worldMaxX, u.k, u.border),
        wall_axis(p.y, u.worldMaxY, u.k, u.border),
        wall_axis(p.z, u.worldMaxZ, u.k, u.border));
    let g: vec3<f32> = vec3<f32>(u.gravityX, u.gravityY, u.gravityZ);

    // Аккумулятивно: и стена, и gravity прибавляются к существующей силе. .w (PE)
    // не трогаем — wall/gravity энергию не пишут (как CPU).
    let prev: vec4<f32> = forces[i];
    forces[i] = vec4<f32>(prev.xyz + fw + g, prev.w);
}
