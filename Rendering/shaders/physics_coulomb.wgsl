// Coulomb pair force compute kernel (per-atom gather over the resident NL).
//
// Зеркалит CPU-владельца кулоновской силы (CoulombForceField::pairInteraction,
// CoulombForceField.h:16-48), вызываемого из общего pair-loop (ForceField.cpp:57-82).
// Структурно — копия compute_lj (physics_lj.wgsl): per-atom Full-NL gather по ТОМУ
// ЖЕ резидентному NL (nlOffsets/nlNeighbors), тот же cutoff, тот же fixed-skip;
// отличие — формула силы (электростатика вместо LJ) и таблица зарядов вместо LJ-типов.
//
// Per-atom gather (паттерн LJ, physics_lj.wgsl:6-8): каждый thread обрабатывает
// одного МОБИЛЬНОГО атома i, СУММИРУЕТ вклад его соседей j и пишет ТОЛЬКО свой
// forces[i] — race-free без f32-атомиков. Newton-3 держится глобально (каждая пара
// (i,j) встречается в Full NL дважды, по разу с каждой стороны, с обратным знаком).
//
// Charge-gated (CoulombForceField действует только на заряженные атомы): центр с
// зарядом 0 не накапливает ничего (chargeA == 0 -> return), сосед с зарядом 0
// пропускается (chargeB == 0 -> continue). Заряды по умолчанию 0, нетривиальные
// приходят из сцены — на нейтральной сцене эта сила добавляет ровно 0.
//
// Контракт буферов (отдельный layout, см. GpuResidentPhysics):
//   - uniforms (binding 0): cutoffSqr, epsilon, mobileCount, kCoulomb
//   - positions (binding 1): read-only AoS vec4<f32> (x,y,z,pad), как LJ binding 1
//   - charges (binding 2): read-only array<f32>, длина N (вместо LJ typeIndices)
//   - nlOffsets (binding 3): read-only u32, длина N+1, CSR (ТОТ ЖЕ, что читает LJ)
//   - nlNeighbors (binding 4): read-only u32, длина nlOffsets[N] (ТОТ ЖЕ, что LJ)
//   - forces (binding 5): read_write vec4<f32> (fx,fy,fz,pe), как LJ binding 6.
//     Должен быть обнулён до dispatch; kernel ПРИБАВЛЯЕТ к .xyz И к .w.

struct CoulombUniforms {
    cutoffSqr: f32,
    epsilon: f32,
    mobileCount: u32,
    kCoulomb: f32, // == CoulombForceField.h:14 (140.399645 eV*A/e^2)
};

@group(0) @binding(0) var<uniform> u: CoulombUniforms;
@group(0) @binding(1) var<storage, read> positions: array<vec4<f32>>;
@group(0) @binding(2) var<storage, read> charges: array<f32>;
@group(0) @binding(3) var<storage, read> nlOffsets: array<u32>;
@group(0) @binding(4) var<storage, read> nlNeighbors: array<u32>;
@group(0) @binding(5) var<storage, read_write> forces: array<vec4<f32>>;

@compute @workgroup_size(64)
fn compute_coulomb(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.mobileCount) {
        return;
    }

    // Центр без заряда не даёт ни одного слагаемого (CPU chargeA-short-circuit,
    // ForceField.cpp:47-55,76-77). Точное f32-сравнение с 0: заряды заливаются как
    // есть из CPU (без арифметики) → бит-идентичны → гард срабатывает одинаково.
    let chargeA: f32 = charges[i];
    if (chargeA == 0.0) {
        return;
    }

    let pi: vec3<f32> = positions[i].xyz;
    let begin: u32 = nlOffsets[i];
    let end: u32 = nlOffsets[i + 1u];

    var force: vec3<f32> = vec3<f32>(0.0, 0.0, 0.0);
    var pe: f32 = 0.0;

    for (var p: u32 = begin; p < end; p = p + 1u) {
        let j: u32 = nlNeighbors[p];
        // Fixed neighbors (индексы >= mobileCount) — декоративные (physics_lj.wgsl:52-55).
        if (j >= u.mobileCount) {
            continue;
        }
        // Сосед без заряда не даёт вклада (CoulombForceField.h:19-21).
        let chargeB: f32 = charges[j];
        if (chargeB == 0.0) {
            continue;
        }

        let pj: vec3<f32> = positions[j].xyz;
        let dr: vec3<f32> = pj - pi; // neighbor - center (ForceField.cpp:63-65), как LJ
        let d2: f32 = dot(dr, dr);
        // cutoff (d2 > cutoffSqr) + epsilon-гард (d2 <= Consts::Epsilon, CoulombForceField.h:23).
        if (d2 > u.cutoffSqr || d2 <= u.epsilon) {
            continue;
        }

        let qqScale: f32 = u.kCoulomb * chargeA * chargeB; // CoulombForceField.h:27 (несёт знак)
        // 1.0 / sqrt(d2), НЕ inverseSqrt — повторяет CPU 1.0f/std::sqrt(d2)
        // (CoulombForceField.h:28) ближе к биту, экономит tolerance-бюджет паритета.
        let invR: f32 = 1.0 / sqrt(d2);
        let forceScale: f32 = qqScale * invR / d2; // CoulombForceField.h:29 (= k*qa*qb / r^3)
        let potential: f32 = qqScale * invR;        // CoulombForceField.h:30 (= k*qa*qb / r)

        // forceX/Y/Z -= dr*forceScale (CoulombForceField.h:32-34,37-39): сила на центр
        // направлена -dr (dr = сосед-центр), отталкивание при qqScale>0, притяжение <0.
        let pairForce: vec3<f32> = dr * forceScale;
        force = force - pairForce;
        // PE += 0.5*potential (CoulombForceField.h:35,40): Full NL встречает пару дважды,
        // каждая встреча кладёт половину → сумма по обоим центрам = полная энергия пары.
        pe = pe + 0.5 * potential;
    }

    // ПРИБАВЛЯЕМ к .xyz И к .w (НЕ устанавливаем). compute_lj тоже прибавляет к .w
    // (physics_lj.wgsl:84-85), поэтому после zero->wall->LJ->coulomb лейн .w несёт
    // LJ_PE + Coulomb_PE. Запись `= pe` (set) затёрла бы LJ_PE — ОБЯЗАН add.
    let prev: vec4<f32> = forces[i];
    forces[i] = vec4<f32>(prev.xyz + force, prev.w + pe);
}
