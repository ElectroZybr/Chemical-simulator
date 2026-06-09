// GPU-редукция максимального смещения атома от reference-позиции (на момент
// последней NL build). Нужна, чтобы решить нужен ли NL rebuild, БЕЗ скачивания
// всех позиций на CPU — читается только 4 байта (флаг).
//
// CPU-аналог: NeighborList::needsRebuild сравнивает |pos - refPos|^2 с
// (0.5*skin)^2 по mobile-атомам (NeighborList.cpp). Здесь тот же расчёт, но
// max собирается atomicMax'ом.
//
// Трюк: disp^2 >= 0 всегда, а IEEE-754 битовое представление неотрицательных
// float монотонно как u32 — значит atomicMax над bitcast<u32> даёт корректный
// максимум. (WGSL не умеет atomic<f32>, только u32/i32.)

struct DispUniforms {
    mobileCount: u32,
};

@group(0) @binding(0) var<uniform> u: DispUniforms;
@group(0) @binding(1) var<storage, read> positions: array<vec4<f32>>;
@group(0) @binding(2) var<storage, read> refPos: array<vec4<f32>>;
@group(0) @binding(3) var<storage, read_write> maxFlag: atomic<u32>;

@compute @workgroup_size(64)
fn reset_flag(@builtin(global_invocation_id) gid: vec3<u32>) {
    if (gid.x == 0u) {
        atomicStore(&maxFlag, 0u);
    }
}

@compute @workgroup_size(64)
fn max_displacement(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.mobileCount) { return; }
    let d: vec3<f32> = positions[i].xyz - refPos[i].xyz;
    let d2: f32 = dot(d, d);
    atomicMax(&maxFlag, bitcast<u32>(d2));
}
