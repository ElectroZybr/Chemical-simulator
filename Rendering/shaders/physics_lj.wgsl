// LJ pair force compute kernel.
//
// Каждый workgroup-thread обрабатывает одного мобильного атома i: проходит
// по его соседям j из NL (full-list mode), считает LJ-силу с фильтром по
// physical cutoff и накапливает -pairF в forceX/Y/Z атома i. В neighbor
// не пишет — Newton 3rd law держится глобально (каждая пара (i,j) встретится
// дважды, по разу с каждой стороны), но без race на forceX(j).
//
// Контракт буферов соответствует CPU-стороне (Engine::GpuPairForceCompute):
//   - positions: read-only AoS vec4<f32> (x, y, z, pad), длина = N
//   - typeIndices: read-only u32, длина = N, индекс в ljPairs
//   - nlOffsets: read-only u32, длина = N+1, CSR-стиль
//   - nlNeighbors: read-only u32, длина = nlOffsets[N]
//   - forces: read_write vec4<f32> (fx, fy, fz, pe), длина = N. Должен быть
//     обнулён до dispatch — kernel прибавляет (writeNeighbor=false контракт).
//   - ljPairs: read-only vec2<f32>(C6, C12) длиной TypeCount*TypeCount, индексация
//     row-major (typeA * TypeCount + typeB)
//   - uniforms: cutoffSqr, epsilon, mobileCount, typeCount

struct Uniforms {
    cutoffSqr: f32,
    epsilon: f32,
    mobileCount: u32,
    typeCount: u32,
};

@group(0) @binding(0) var<uniform> u: Uniforms;
@group(0) @binding(1) var<storage, read> positions: array<vec4<f32>>;
@group(0) @binding(2) var<storage, read> typeIndices: array<u32>;
@group(0) @binding(3) var<storage, read> nlOffsets: array<u32>;
@group(0) @binding(4) var<storage, read> nlNeighbors: array<u32>;
@group(0) @binding(5) var<storage, read> ljPairs: array<vec2<f32>>;
@group(0) @binding(6) var<storage, read_write> forces: array<vec4<f32>>;

@compute @workgroup_size(64)
fn compute_lj(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.mobileCount) {
        return;
    }

    let pi: vec3<f32> = positions[i].xyz;
    let typeI: u32 = typeIndices[i];
    let begin: u32 = nlOffsets[i];
    let end: u32 = nlOffsets[i + 1u];

    var force: vec3<f32> = vec3<f32>(0.0, 0.0, 0.0);
    var pe: f32 = 0.0;

    for (var p: u32 = begin; p < end; p = p + 1u) {
        let j: u32 = nlNeighbors[p];
        // Fixed neighbors (индексы >= mobileCount) — декоративные, см. Engine docs.
        if (j >= u.mobileCount) {
            continue;
        }

        let pj: vec3<f32> = positions[j].xyz;
        let dr: vec3<f32> = pj - pi;
        let d2: f32 = dot(dr, dr);
        if (d2 > u.cutoffSqr || d2 <= u.epsilon) {
            continue;
        }

        let typeJ: u32 = typeIndices[j];
        let pair: vec2<f32> = ljPairs[typeI * u.typeCount + typeJ];

        let invD2: f32 = 1.0 / d2;
        let invD6: f32 = invD2 * invD2 * invD2;
        let invD12: f32 = invD6 * invD6;
        let term6: f32 = pair.x * invD6;
        let term12: f32 = pair.y * invD12;
        let forceScale: f32 = (12.0 * term12 - 6.0 * term6) * invD2;
        let potential: f32 = term12 - term6;

        // -pairF в central (writeNeighbor=false: на стороне j посчитается с
        // обратным знаком, Newton-3 держится глобально).
        let pairF: vec3<f32> = dr * forceScale;
        force = force - pairF;
        pe = pe + 0.5 * potential;
    }

    // Прибавляем к существующим forces (там уже могут быть wall/initial значения,
    // обнуление delegируется CPU перед dispatch).
    let prev: vec4<f32> = forces[i];
    forces[i] = vec4<f32>(prev.xyz + force, prev.w + pe);
}
