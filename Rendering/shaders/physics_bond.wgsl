// Bond force compute kernels (per-atom gather over bond-adjacency CSR).
//
// Зеркалит CPU-владельца bond-сил (BondForceField::compute → Bond::forceBond,
// Bond.cpp:30-58, и Bond::angleForce, Bond.cpp:77-144). Топология связей резидентна
// в VRAM как CSR (offsets+neighbors), зеркаля nlOffsets_/nlNeighbors_; каждая связь
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
    thetaZero: f32,  // == Bond.cpp:116 (60° в радианах) — равновесный угол, читает compute_bond_angle
    kAngle: f32,     // == Bond.cpp:121 (50.0) — жёсткость угла, читает compute_bond_angle
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

// --- Угловые силы (зеркалит Bond::angleForce, Bond.cpp:77-144) ---------------
//
// CPU `angleForce(o, b, c)`: o=ЦЕНТР (вершина), b,c=два КОНЦА (плечи). Считает
// гармонический угловой потенциал вокруг theta_0 и применяет силу ко ВСЕМ ТРЁМ:
// force_b на плечо b, force_c на плечо c, force_o = -(force_b+force_c) на центр.
// applyAngleForces (BondForceField.cpp:136-146) зовёт angleForce(o, b, c) РОВНО
// один раз на каждую неупорядоченную пару (b,c) bonded-соседей центра o.
//
// На GPU per-atom gather: атом i накапливает СВОЮ угловую силу в ДВУХ ролях
// (compute_bond_angle ниже): роль A — i центр (force_o пар своих рёбер); роль B —
// i плечо (force_b троек, где центр — сосед i). Обе роли используют единый расчёт
// геометрии тройки в порядке CPU (Bond.cpp:88-122), чтобы f32-результат совпал
// побитово с CPU-членом этой тройки.
//
// Тип-обёртка для возврата всех трёх сил тройки из одного расчёта геометрии:
// force_o нужен force_b И force_c одной тройки (Bond.cpp:129-131), поэтому считаем
// их вместе за один проход (как CPU), а не двумя независимыми вызовами.
struct AngleTriplet {
    valid: bool,    // false если сработал гард len<=1e-12 или sin²<1e-12 (сила 0)
    force_b: vec3<f32>, // сила на плечо b (Bond.cpp:123-125)
    force_c: vec3<f32>, // сила на плечо c (Bond.cpp:126-128)
};

// Полный расчёт сил тройки (center, b, c). Порядок операций ТОЧНО по Bond.cpp:88-131
// для f32-паритета: δ → len (гард) → единичные → cos (clamp) → sin²=1-cos² (гард) →
// acos → sin=sqrt(sin²) → force_scale → force_b/force_c. dt не участвует (angle его
// не принимает). При срабатывании гарда возвращаем valid=false (вклад тройки = 0,
// как CPU `return` на Bond.cpp:98,112).
fn angle_triplet(center: u32, b: u32, c: u32) -> AngleTriplet {
    var out: AngleTriplet;
    out.valid = false;
    out.force_b = vec3<f32>(0.0, 0.0, 0.0);
    out.force_c = vec3<f32>(0.0, 0.0, 0.0);

    let po: vec3<f32> = positions[center].xyz;
    let pb: vec3<f32> = positions[b].xyz;
    let pc: vec3<f32> = positions[c].xyz;

    // δ_ob = pos(b) - pos(o), δ_oc = pos(c) - pos(o) (Bond.cpp:88-93).
    let delta_ob: vec3<f32> = pb - po;
    let delta_oc: vec3<f32> = pc - po;

    let len_ob: f32 = length(delta_ob);
    let len_oc: f32 = length(delta_oc);
    // Гард len <= 1e-12 → сила 0 (Bond.cpp:97-99).
    if (len_ob <= 1e-12 || len_oc <= 1e-12) {
        return out;
    }

    // Единичные ob_hat/oc_hat (Bond.cpp:101-106).
    let ob_hat: vec3<f32> = delta_ob / len_ob;
    let oc_hat: vec3<f32> = delta_oc / len_oc;

    // cos_theta = dot(ob_hat, oc_hat), clamp [-1,1] (Bond.cpp:108-109).
    var cos_theta: f32 = dot(ob_hat, oc_hat);
    cos_theta = clamp(cos_theta, -1.0, 1.0);
    // sin²=1-cos²; гард <1e-12 → сила 0 (Bond.cpp:110-113). НЕ sin(acos(...)).
    let sin_theta_sqr: f32 = 1.0 - cos_theta * cos_theta;
    if (sin_theta_sqr < 1e-12) {
        return out;
    }

    let angle_theta: f32 = acos(cos_theta);              // Bond.cpp:115
    let angle_loss: f32 = angle_theta - u.thetaZero;     // theta_0 == Bond.cpp:116
    let sin_theta: f32 = sqrt(sin_theta_sqr);            // Bond.cpp:119
    // force_scale = -k * angle_loss / sin_theta (k == Bond.cpp:121).
    let force_scale: f32 = -u.kAngle * angle_loss / sin_theta;

    // force_b/force_c покомпонентно (Bond.cpp:123-128). Знаменатели РАЗНЫЕ
    // (len_ob vs len_oc), числители тоже — формы НЕ взаимозаменяемы.
    out.force_b = -((oc_hat - ob_hat * cos_theta) / len_ob) * force_scale;
    out.force_c = -((ob_hat - oc_hat * cos_theta) / len_oc) * force_scale;
    out.valid = true;
    return out;
}

// Сила на ЦЕНТР тройки (center, b, c): force_o = -(force_b+force_c) (Bond.cpp:129-131).
fn angle_force_on_center(center: u32, b: u32, c: u32) -> vec3<f32> {
    let t: AngleTriplet = angle_triplet(center, b, c);
    if (!t.valid) {
        return vec3<f32>(0.0, 0.0, 0.0);
    }
    return -(t.force_b + t.force_c);
}

// Сила на ПЛЕЧО arm тройки (center, arm, other): force_b-форма с arm на позиции b
// (Bond.cpp:123-125). Q-A1 (ПОДТВЕРЖДЁН): сила на плечо зависит только от (центр,
// СВОЁ плечо, ДРУГОЕ плечо), а force_scale симметричен — поэтому эта форма даёт
// верный CPU-член НЕЗАВИСИМО от того, был ли arm помечен b или c в CPU-нумерации
// (если arm был c, CPU-член force_c с len_oc=len_o_arm совпадает с этой force_b-формой
// при подстановке arm=b, other=c). Метка b/c в CSR НЕ нужна (дизайн §2.3).
fn angle_force_on_arm(center: u32, arm: u32, other: u32) -> vec3<f32> {
    let t: AngleTriplet = angle_triplet(center, arm, other);
    if (!t.valid) {
        return vec3<f32>(0.0, 0.0, 0.0);
    }
    return t.force_b;
}

// Угловая сила на атом i (per-atom gather, две роли). Диспатч по gTotal (как Morse),
// гард i >= totalCount. Читает positions + bondOffsets + bondNeighbors (НЕ bondParams
// — угол берёт только геометрию + глобальные theta_0/k из uniform). Прибавляет к
// forces[i].xyz; .w (PE) не трогает.
//
// Двух-ролевой gather (дизайн §2.3, паритет доказан §2.3): каждый член каждой тройки
// (force_b, force_c, force_o) попадает ровно одному атому ровно один раз. Сумма по
// всем атомам обеих ролей == CPU-сумма applyAngleForces (поатомно распределённые
// force_b+force_c+force_o каждой тройки).
@compute @workgroup_size(64)
fn compute_bond_angle(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i: u32 = gid.x;
    if (i >= u.totalCount) {
        return;
    }

    var force: vec3<f32> = vec3<f32>(0.0, 0.0, 0.0);
    let bi: u32 = bondOffsets[i];
    let ei: u32 = bondOffsets[i + 1u];

    // --- Роль A: i ЦЕНТР. Для каждой неупорядоченной пары (b,c) своих рёбер
    //     (p<q по позиции в CSR-окне, зеркалит i<j в BondForceField.cpp:142-143)
    //     прибавляем force_o тройки (i, b, c). Локально: только positions[i,b,c].
    for (var p: u32 = bi; p < ei; p = p + 1u) {
        let bnb: u32 = bondNeighbors[p];
        for (var q: u32 = p + 1u; q < ei; q = q + 1u) {
            let cnb: u32 = bondNeighbors[q];
            force = force + angle_force_on_center(i, bnb, cnb);
        }
    }

    // --- Роль B: i ПЛЕЧО. Для каждого своего bonded-соседа o (центр тройки)
    //     перебираем ДРУГИХ соседей o (c != i, двух-хоповый обход CSR соседа o)
    //     и прибавляем force_b тройки (center=o, arm=i, other=c). Каждая такая
    //     тройка посещается CPU ровно раз (angleForce(o, *, *) на пару (i,c)),
    //     здесь i получает СВОЙ force_b ровно раз.
    for (var p: u32 = bi; p < ei; p = p + 1u) {
        let o: u32 = bondNeighbors[p];
        let bo: u32 = bondOffsets[o];
        let eo: u32 = bondOffsets[o + 1u];
        for (var q: u32 = bo; q < eo; q = q + 1u) {
            let cnb: u32 = bondNeighbors[q];
            if (cnb == i) {
                continue; // c — другое плечо, не сам i (исключаем ребро o→i)
            }
            force = force + angle_force_on_arm(o, i, cnb);
        }
    }

    // .w (PE) не трогаем — bonds энергию не пишут. Прибавляем к силе (zero→wall→
    // LJ→morse уже отработали).
    let prev: vec4<f32> = forces[i];
    forces[i] = vec4<f32>(prev.xyz + force, prev.w);
}
