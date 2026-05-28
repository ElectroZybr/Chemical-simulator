// Velocity Verlet integrator на GPU. Энтри-поинты точно зеркалят CPU:
//   VerletScheme::predict / correct (VerletScheme.cpp) и StepOps::confineToBox.
//
// Резидентный шаг (GPU mode) повторяет CPU-порядок:
//   predict(mobile) -> confine(mobile) -> [swap pf<->f на CPU-стороне через
//   bind-group parity] -> zero_forces(total) -> LJ(mobile) -> correct(mobile)
//
// predict/confine/correct диспатчатся над mobileCount; zero_forces — над
// totalCount (CPU зануляет силы по size(), не mobileCount — StepOps.h:89-92,
// чтобы fixed-атомы не накапливали мусор).
//
// Буфера vec4: positions (x,y,z,_), velocities (vx,vy,vz,_), forces (fx,fy,fz,pe),
// prevForces (pfx,pfy,pfz,_). invMass: f32. Лейн .w позиций/скоростей не трогаем.

struct IntegratorUniforms {
    dt: f32,
    accelDamping: f32,
    worldMaxX: f32,   // worldSize - 1, per StepOps.h:22
    worldMaxY: f32,
    worldMaxZ: f32,
    restitution: f32, // 0.8, StepOps.h:21
    mobileCount: u32,
    totalCount: u32,
};

@group(0) @binding(0) var<uniform> u: IntegratorUniforms;
@group(0) @binding(1) var<storage, read_write> positions: array<vec4<f32>>;
@group(0) @binding(2) var<storage, read_write> velocities: array<vec4<f32>>;
@group(0) @binding(3) var<storage, read_write> forces: array<vec4<f32>>;
@group(0) @binding(4) var<storage, read> prevForces: array<vec4<f32>>;
@group(0) @binding(5) var<storage, read> invMass: array<f32>;

// x += (v + f*invMass*0.5*dt) * dt   (VerletScheme.cpp:35-37)
@compute @workgroup_size(64)
fn predict(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.mobileCount) { return; }
    let im: f32 = invMass[i];
    let v: vec3<f32> = velocities[i].xyz;
    let f: vec3<f32> = forces[i].xyz;
    let dx: vec3<f32> = (v + f * im * 0.5 * u.dt) * u.dt;
    positions[i] = vec4<f32>(positions[i].xyz + dx, positions[i].w);
}

// confineToBox: clamp [0, max], reflect velocity ×restitution ТОЛЬКО когда
// атом движется в стену (directional guard, StepOps.h:26-39).
@compute @workgroup_size(64)
fn confine(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.mobileCount) { return; }
    var p: vec3<f32> = positions[i].xyz;
    var v: vec3<f32> = velocities[i].xyz;
    let maxes: vec3<f32> = vec3<f32>(u.worldMaxX, u.worldMaxY, u.worldMaxZ);

    for (var a: u32 = 0u; a < 3u; a = a + 1u) {
        if (p[a] < 0.0) {
            p[a] = 0.0;
            if (v[a] < 0.0) { v[a] = -v[a] * u.restitution; }
        } else if (p[a] > maxes[a]) {
            p[a] = maxes[a];
            if (v[a] > 0.0) { v[a] = -v[a] * u.restitution; }
        }
    }
    positions[i] = vec4<f32>(p, positions[i].w);
    velocities[i] = vec4<f32>(v, velocities[i].w);
}

// Зануление forces (включая .w = pe) над ВСЕМИ атомами (total, не mobile) —
// CPU делает fill_n по size() (StepOps.h:89-92).
@compute @workgroup_size(64)
fn zero_forces(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.totalCount) { return; }
    forces[i] = vec4<f32>(0.0, 0.0, 0.0, 0.0);
}

// halfDtInvMass = 0.5 * accelDamping * dt * invMass; v += (pf + f) * halfDtInvMass
// (VerletScheme.cpp:61-65). prevForces (pf) хранит силу прошлого шага.
@compute @workgroup_size(64)
fn correct(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.mobileCount) { return; }
    let halfDtInvMass: f32 = 0.5 * u.accelDamping * u.dt * invMass[i];
    let f: vec3<f32> = forces[i].xyz;       // сила текущего шага (после LJ)
    let pf: vec3<f32> = prevForces[i].xyz;  // сила прошлого шага (после swap)
    let v: vec3<f32> = velocities[i].xyz;
    velocities[i] = vec4<f32>(v + (pf + f) * halfDtInvMass, velocities[i].w);
}
