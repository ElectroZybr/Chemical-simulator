// Parity-гейт переноса Coulomb pair-сил на GPU (фаза 2.3). GPU-траектория с
// заряженными атомами должна совпасть с CPU velocity Verlet (ForceField →
// CoulombForceField::pairInteraction) в пределах fp-tolerance. Без этого «GPU
// считает кулоновские силы» — недоказанное утверждение. Это ФИНАЛЬНАЯ сила Фазы 2
// (после 2.1 walls+gravity, 2.2 bonds Morse+angle).
//
// Coulomb charge-gated: действует ТОЛЬКО на атомы с НЕнулевым зарядом (заряды по
// умолчанию 0). Поэтому гейт ОБЯЗАН явно ВЫСТАВИТЬ ненулевые СМЕШАННЫЕ +/- заряды
// (иначе тестировал бы 0==0 — слепо, как Morse-в-равновесии-r0). Заряды задаются
// прямым присвоением atoms.charge(i)=q после fill + notifySceneEdited (заливка в
// VRAM при входе в GPU-режим).
//
// Ключевые свойства гейта (требования дизайна §6, §11):
//   (a) Комбинированный LJ+Coulomb PE-assert (ловит баг .w SET-вместо-ADD):
//       LJ ВКЛ + Coulomb ВКЛ + заряды; после шагов читаем резидентный
//       лейн .w (readbackPotentialEnergy), суммируем мобильные .w, сверяем с CPU
//       суммой atoms.energy(). ОБЯЗАН быть LJ+Coulomb (не Coulomb-only): при LJ-ON
//       prev.w=LJ_PE≠0, и SET (=pe) затёр бы LJ_PE → провал; Coulomb-only (prev.w=0)
//       не различил бы SET/ADD. Порог assert'а АБСОЛЮТНЫЙ и привязан к измеренному
//       LJ_PE-сигналу (величине, которую SET затирает): absdiff < 0.25·|LJ_PE|. NB:
//       относительный порог по ПОЛНОЙ PE был бы слеп — LJ_PE мал на фоне Coulomb_PE,
//       SET сдвинул бы полную PE на |LJ_PE|/|PE|≈6e-4 < типичного rel-порога. Плюс
//       blindness-guard: |LJ_PE| должен быть значим, иначе гейт слеп к SET.
//   (b) Coulomb-toggle регрессия: заряженная сцена, Coulomb ВКЛ,
//       setGpuMode, прогон; setCoulombEnabled(false) на CPU+GPU; прогон; сверка с
//       эталоном «Coulomb остался ВКЛ» (stale-ref), parity << staleGapRef (форма
//       !(parity < k*staleGap) ловит и NaN — как lj-toggle 2.2a). Ловит stale-флаг.
//   (c) Trajectory parity: GPU vs CPU max|Δpos| (Coulomb-only И комбинированный).
//       + ASSERT CPU реально двинул заряженные атомы (иначе гейт слеп).
//   (d) No-regression: uncharged-сцена + Coulomb ВКЛ → Coulomb добавляет ровно 0
//       (chargeA==0 short-circuit) → GPU траектория == CPU. (BM_GpuCorrectness с
//       зарядами 0 это тоже подтверждает отдельно — двойная защита.)
//   (e) Charge-readback: резидентные charges_ читаются обратно (readbackCharges) и
//       сверяются с CPU atoms.charge() ТОЧНО — ловит «заряды не залиты» (ложный pass).
//
// GPU-сторона гоняется через РЕАЛЬНЫЙ путь Simulation::setGpuMode(true)+updateAll()
// (не голый GpuResidentPhysics), чтобы тестировать uploadSceneToGpu→uploadFromCpu
// доставку charges_ И coulombEnabled_ — тот же путь, что у приложения.
//
// Численность: Coulomb весь во float на CPU (CoulombForceField.h:27-40), GPU всё в
// f32 → дрейф уровня LJ-паритета (нет double-геометрии, как у angle). Tolerance ПО
// ИЗМЕРЕНИЮ, провенанс в RESULTS.md.
//
// Это не gtest (latticelab_tests не поднимает WGPU device); живёт в bench-бинаре
// (benchmarkDevice). Бросает при расхождении/нарушении инварианта — прогон падает.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <vector>

#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h" // benchmarkDevice()
#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/Simulation.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"
using namespace Lattice;

namespace {

constexpr float kDt = 0.01f;
constexpr float kAccelDamping = 0.9f;
constexpr int kCellSize = 6;
constexpr float kWorldSize = 24.0f; // max=23; атомы в районе центра, далеко от стен
constexpr int kSteps = 50;

// Атом сцены: тип (несёт массу + LJ-параметры) + позиция + заряд. Заряд задаётся
// после вставки через atoms.charge(i)=q (UI для заряда нет; нетривиальные заряды
// приходят из сцены — здесь имитируем явным присвоением).
struct ChargedAtomSpec {
    AtomData::Type t;
    Vec3f p;
    float charge;
};

// Пара притяжения (+1/-1) + пара отталкивания (+1/+1), смешанные заряды, в пределах
// cutoff(5) друг от друга ВНУТРИ пары, пары разнесены по Y на 6 (>cutoff) → межпарных
// Coulomb-соседей нет (две чистые пары).
//
// КРИТИЧНО для устойчивости (комбинированный LJ+Coulomb кейс): расстояние В ПАРЕ = 3.0
// (НЕ 1.5). LJ-минимум H≈1.122·a0=2.69 (a0=2.40, AtomData.cpp:8); на 3.0 LJ за
// репульсивной стеной → сила/энергия КОНЕЧНЫ и сцена устойчива за 50 шагов (как
// решётка spacing=3.0 в BM_GpuCorrectness/lj-toggle). На 1.5 H-H были бы ГЛУБОКО в
// LJ-стене → взрыв (NaN-позиции, разлёт за cutoff → PE схлопывается в 0, паритет
// тривиально 0, PE-assert вырождается в 0==0). При 3.0 и LJ_PE, и Coulomb_PE НЕнулевы
// → PE-assert РЕАЛЬНО различает .w SET/ADD. Coulomb на 3.0 (k·1·1/9≈15.6) умерен →
// устойчив. H/H (равные массы) — паритет всё равно ловится (дизайн §6.4: минимум —
// смешанные заряды; массы можно оставить равными).
std::vector<ChargedAtomSpec> chargedSpecs() {
    return {
        // Пара притяжения: разноимённые заряды (+1/-1 → qqScale<0), расстояние 4.5.
        // В Coulomb-only (LJ off) притяжение СБЛИЖАЕТ атомы — старт 4.5 (а не 3.0)
        // держит их вдали от epsilon-сингулярности 1/r^3 за прогон (на малых r f32-
        // дрейф 1/d2^{3/2} усиливается, дизайн §6.7). 4.5 < cutoff(5) → Coulomb активен.
        {AtomData::Type::H, Vec3f{6.5f, 8.0f, 8.0f}, +1.0f},
        {AtomData::Type::H, Vec3f{11.0f, 8.0f, 8.0f}, -1.0f},
        // Пара отталкивания: одноимённые заряды (+1/+1 → qqScale>0), расстояние 4.5.
        // Отталкивание РАЗВОДИТ атомы (прочь от сингулярности) → численно устойчиво.
        {AtomData::Type::H, Vec3f{6.5f, 14.0f, 8.0f}, +1.0f},
        {AtomData::Type::H, Vec3f{11.0f, 14.0f, 8.0f}, +1.0f},
    };
}

// Общая конфигурация. ljEnabled/coulombEnabled — параметры (изоляция Coulomb-only vs
// комбинированный). Формация ВЫКЛ (Coulomb не зависит от связей), gravity=0, атомы
// вдали от стен (wall=0).
void configureSim(Simulation& sim, bool ljEnabled, bool coulombEnabled) {
    sim.createWorld(Vec3f{kWorldSize, kWorldSize, kWorldSize});
    sim.setSizeBox(sim.world().getWorldSize(), kCellSize);
    sim.setLJEnabled(ljEnabled);
    sim.setCoulombEnabled(coulombEnabled);
    sim.setBondFormationEnabled(false);
    sim.setGravity(Vec3f{0.0f, 0.0f, 0.0f});
    sim.setDt(kDt);
    sim.setAccelDamping(kAccelDamping);
}

// Заполняет sim атомами + выставляет заряды. Порядок детерминирован (индекс i →
// specs[i]). Заряды ставятся ПОСЛЕ finalizeAtomBatch (индексы стабильны) + ещё один
// notifySceneEdited, чтобы при входе в GPU-режим charges_ залились в VRAM.
void fillCharged(Simulation& sim, const std::vector<ChargedAtomSpec>& specs) {
    for (const ChargedAtomSpec& s : specs) {
        sim.appendAtomFast(s.p, Vec3f{0.0f, 0.0f, 0.0f}, s.t, false);
    }
    sim.finalizeAtomBatch();
    AtomStorage& a = sim.atoms();
    for (size_t i = 0; i < specs.size(); ++i) {
        a.charge(i) = specs[i].charge;
    }
    sim.notifySceneEdited(); // заряды изменены вне обычного шага → бамп версии → re-upload charges_
}

// max|pos_a - pos_b| по всем атомам (порядок детерминирован — сцены идентичны).
double maxAbsPositionDiff(const AtomStorage& a, const AtomStorage& b) {
    double maxAbs = 0.0;
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        for (int c = 0; c < 3; ++c) {
            const float av = (c == 0) ? a.posX(i) : (c == 1) ? a.posY(i) : a.posZ(i);
            const float bv = (c == 0) ? b.posX(i) : (c == 1) ? b.posY(i) : b.posZ(i);
            maxAbs = std::max(maxAbs, std::abs(static_cast<double>(av) - static_cast<double>(bv)));
        }
    }
    return maxAbs;
}

// max|pos - startPos| по всем атомам (для assert'а «сила реально двинула атомы»).
double maxSelfDisplacement(const AtomStorage& a, const std::vector<ChargedAtomSpec>& specs) {
    double maxDisp = 0.0;
    for (size_t i = 0; i < specs.size(); ++i) {
        const Vec3f sp = specs[i].p;
        maxDisp = std::max(maxDisp, std::abs(static_cast<double>(a.posX(i)) - sp.x));
        maxDisp = std::max(maxDisp, std::abs(static_cast<double>(a.posY(i)) - sp.y));
        maxDisp = std::max(maxDisp, std::abs(static_cast<double>(a.posZ(i)) - sp.z));
    }
    return maxDisp;
}

// Сумма лейна .w (PE) по МОБИЛЬНЫМ атомам резидентного force-буфера.
double sumResidentMobilePe(const GpuResidentPhysics& gpu, size_t mobileCount) {
    const std::vector<float> w = gpu.readbackPotentialEnergy();
    double sum = 0.0;
    const size_t n = std::min(mobileCount, w.size());
    for (size_t i = 0; i < n; ++i) {
        sum += static_cast<double>(w[i]);
    }
    return sum;
}

// Сумма CPU atoms.energy() по МОБИЛЬНЫМ атомам (PE-аккумулятор pair-loop'а).
double sumCpuMobilePe(const AtomStorage& a) {
    double sum = 0.0;
    for (size_t i = 0; i < a.mobileCount(); ++i) {
        sum += static_cast<double>(a.energy(i));
    }
    return sum;
}

// --- (a)+(c-combined)+(e): комбинированный LJ+Coulomb прогон --------------------
// LJ ВКЛ + Coulomb ВКЛ + заряды. Проверяет: PE-лейн копит LJ_PE+Coulomb_PE (a),
// траектория складывает обе силы (c), заряды доставлены (e).
struct CombinedResult {
    double cpuSelfDisplacement; // CPU реально двинулся (силы НЕнулевы — не слепо)
    double trajMaxAbs;          // GPU vs CPU max|Δpos| (комбинированный)
    double gpuPeSum;            // сумма резидентного .w (LJ_PE + Coulomb_PE)
    double cpuPeSum;            // сумма CPU atoms.energy()
    bool chargesMatch;          // (e) резидентные charges_ == CPU заряды
    double peAbsDiff;           // |gpuPeSum - cpuPeSum|
    double peScale;             // max(|cpu|,|gpu|) — масштаб (для диагностики)
    double ljPeSignal;          // |LJ_PE| на финальных позициях — ровно та величина,
                                // которую затёр бы баг .w SET-вместо-ADD (порог assert'а)
};

CombinedResult runCombined() {
    const std::vector<ChargedAtomSpec> specs = chargedSpecs();
    const size_t atomCount = specs.size();

    Simulation cpu;
    configureSim(cpu, /*lj=*/true, /*coulomb=*/true);
    fillCharged(cpu, specs);

    Simulation gpu;
    configureSim(gpu, /*lj=*/true, /*coulomb=*/true);
    fillCharged(gpu, specs);
    gpu.setGpuMode(true);

    // (e) Сразу после входа в GPU-режим: резидентные charges_ == CPU-заряды (точно).
    bool chargesMatch = true;
    {
        const GpuResidentPhysics* res = gpu.activeGpuResident();
        if (res == nullptr) {
            throw std::runtime_error("CoulombParity: GPU resident missing after setGpuMode");
        }
        const std::vector<float> gotCharges = res->readbackCharges();
        if (gotCharges.size() != atomCount) {
            std::printf("[ COULPAR  ] charges len %zu != atomCount %zu\n", gotCharges.size(), atomCount);
            throw std::runtime_error("CoulombParity: resident charges length mismatch");
        }
        for (size_t i = 0; i < atomCount; ++i) {
            if (gotCharges[i] != specs[i].charge) {
                std::printf("[ COULPAR  ] charge[%zu] %.6f != expected %.6f\n", i, gotCharges[i], specs[i].charge);
                chargesMatch = false;
            }
        }
    }

    for (int s = 0; s < kSteps; ++s) {
        cpu.updateAll();
        gpu.updateAll();
    }
    // Сумма резидентного .w ДО syncFromGpuIfNeeded (та качает только pos/vel, .w не
    // трогает — но читаем тут явно по текущему parity_, пока ничего не сдвинуло шаг).
    double gpuPeSum = 0.0;
    {
        const GpuResidentPhysics* res = gpu.activeGpuResident();
        if (res == nullptr) {
            throw std::runtime_error("CoulombParity: GPU resident missing after run");
        }
        gpuPeSum = sumResidentMobilePe(*res, gpu.atoms().mobileCount());
    }
    gpu.syncFromGpuIfNeeded(); // стянуть резидентные позиции для сверки траектории

    // Измеряем ИМЕННО LJ_PE на финальных позициях комбинированного прогона — ровно ту
    // величину, которую баг .w SET-вместо-ADD затёр бы из лейна .w. Та же сцена/конфиг,
    // но LJ ВКЛ / Coulomb ВЫКЛ; атомы вставляем на ФИНАЛЬНЫХ позициях CPU-прогона. Один
    // updateAll: predict при v=0,f=0 не двигает (свежая sim) → confine → forces+energy
    // занулены (StepOps::predictAndSync) → LJ pair-loop кладёт LJ_PE в energy(). Так порог
    // assert'а привязан к реальному сигналу, а не к масштабу полной PE.
    double ljPeSignal = 0.0;
    {
        Simulation tmp;
        configureSim(tmp, /*lj=*/true, /*coulomb=*/false);
        const AtomStorage& fin = cpu.atoms(); // финальные позиции комбинированного прогона
        for (size_t i = 0; i < atomCount; ++i) {
            tmp.appendAtomFast(Vec3f{fin.posX(i), fin.posY(i), fin.posZ(i)}, Vec3f{0.0f, 0.0f, 0.0f}, specs[i].t,
                               /*fixed=*/false);
        }
        tmp.finalizeAtomBatch();
        AtomStorage& ta = tmp.atoms();
        for (size_t i = 0; i < atomCount; ++i) {
            ta.charge(i) = specs[i].charge; // как в сцене (на LJ_PE не влияет: Coulomb ВЫКЛ)
        }
        tmp.notifySceneEdited();
        tmp.updateAll(); // заполняет energy() = LJ_PE на этих позициях
        ljPeSignal = sumCpuMobilePe(tmp.atoms());
    }

    CombinedResult r{};
    r.cpuSelfDisplacement = maxSelfDisplacement(cpu.atoms(), specs);
    r.trajMaxAbs = maxAbsPositionDiff(cpu.atoms(), gpu.atoms());
    r.gpuPeSum = gpuPeSum;
    r.cpuPeSum = sumCpuMobilePe(cpu.atoms());
    r.chargesMatch = chargesMatch;
    r.peAbsDiff = std::abs(r.gpuPeSum - r.cpuPeSum);
    r.peScale = std::max(std::abs(r.cpuPeSum), std::abs(r.gpuPeSum));
    r.ljPeSignal = ljPeSignal;
    std::printf("[ COULPAR  ] combined LJ+Coulomb: steps=%d CPU-self-disp=%.3e max|dCPU-GPU|=%.3e abs "
                "| PE gpu=%.6e cpu=%.6e absdiff=%.3e LJ_PE-signal=%.6e\n",
                kSteps, r.cpuSelfDisplacement, r.trajMaxAbs, r.gpuPeSum, r.cpuPeSum, r.peAbsDiff, r.ljPeSignal);
    return r;
}

// --- (c-only): Coulomb-only прогон (LJ ВЫКЛ) ------------------------------------
// Изолирует Coulomb-ядро: ЕДИНСТВЕННАЯ сила — Coulomb → траектория несёт ИМЕННО его.
// Проверяет §5-доставку: GPU считает Coulomb (включён) и НЕ считает LJ (выключен,
// ljEnabled_=false долетел через 2.2a).
struct CoulombOnlyResult {
    double cpuSelfDisplacement;
    double trajMaxAbs;
};

CoulombOnlyResult runCoulombOnly() {
    const std::vector<ChargedAtomSpec> specs = chargedSpecs();

    Simulation cpu;
    configureSim(cpu, /*lj=*/false, /*coulomb=*/true);
    fillCharged(cpu, specs);

    Simulation gpu;
    configureSim(gpu, /*lj=*/false, /*coulomb=*/true);
    fillCharged(gpu, specs);
    gpu.setGpuMode(true);

    for (int s = 0; s < kSteps; ++s) {
        cpu.updateAll();
        gpu.updateAll();
    }
    gpu.syncFromGpuIfNeeded();

    CoulombOnlyResult r{};
    r.cpuSelfDisplacement = maxSelfDisplacement(cpu.atoms(), specs);
    r.trajMaxAbs = maxAbsPositionDiff(cpu.atoms(), gpu.atoms());
    std::printf("[ COULPAR  ] coulomb-only : steps=%d CPU-self-disp=%.3e max|dCPU-GPU|=%.3e abs\n", kSteps,
                r.cpuSelfDisplacement, r.trajMaxAbs);
    return r;
}

// --- (d): no-regression — uncharged сцена + Coulomb ВКЛ → добавляет ровно 0 -----
// ВСЕ заряды 0, Coulomb ВКЛ. compute_coulomb при chargeA==0 делает return сразу →
// атом не накапливает Coulomb → траектория == CPU (Coulomb добавил ровно 0). LJ ВКЛ,
// чтобы атомы вообще двигались (нужен ненулевой self-disp, иначе тест слеп). Это
// доказывает, что charge-0 атомы дают точный 0 (как пустой bond-CSR в 2.2).
struct UnchargedResult {
    double cpuSelfDisplacement;
    double trajMaxAbs;
};

UnchargedResult runUncharged() {
    std::vector<ChargedAtomSpec> specs = chargedSpecs();
    for (ChargedAtomSpec& s : specs) {
        s.charge = 0.0f; // обнуляем заряды — Coulomb должен добавить ровно 0
    }

    Simulation cpu;
    configureSim(cpu, /*lj=*/true, /*coulomb=*/true); // Coulomb ВКЛ, но заряды 0 → 0 вклада
    fillCharged(cpu, specs);

    Simulation gpu;
    configureSim(gpu, /*lj=*/true, /*coulomb=*/true);
    fillCharged(gpu, specs);
    gpu.setGpuMode(true);

    for (int s = 0; s < kSteps; ++s) {
        cpu.updateAll();
        gpu.updateAll();
    }
    gpu.syncFromGpuIfNeeded();

    UnchargedResult r{};
    r.cpuSelfDisplacement = maxSelfDisplacement(cpu.atoms(), specs);
    r.trajMaxAbs = maxAbsPositionDiff(cpu.atoms(), gpu.atoms());
    std::printf("[ COULPAR  ] uncharged(0) : steps=%d CPU-self-disp=%.3e max|dCPU-GPU|=%.3e abs (Coulomb adds 0)\n",
                kSteps, r.cpuSelfDisplacement, r.trajMaxAbs);
    return r;
}

// --- (b): Coulomb-toggle регрессия на рантайм-доставку setCoulombEnabled --------
// GPU-шаг диспатчит Coulomb под флагом coulombEnabled_, снятым на upload. Если
// setCoulombEnabled НЕ бампит cpuSceneVersion, выключение Coulomb ПОСЛЕ setGpuMode
// не долетает до GPU (step продолжает считать Coulomb по устаревшему флагу) →
// дивергенция с CPU. Заряженная сцена, Coulomb ВКЛ старт; прогон, потом
// setCoulombEnabled(false) на ОБЕИХ sim, прогон, сверка. Эталон cpuStale (Coulomb
// остаётся ВКЛ) даёт масштаб разрыва — если бы GPU проигнорил toggle, повёл бы себя
// как cpuStale. (LJ ВЫКЛ: тогда toggle Coulomb — единственный источник расхождения,
// эффект чистый.)
struct CoulombToggleResult {
    double parity;      // GPU(toggle) vs CPU(toggle) — должно быть мало (toggle долетел)
    double staleGapRef; // CPU(toggle) vs CPU(Coulomb остался ВКЛ) — масштаб «если бы проигнорил»
};

CoulombToggleResult runCoulombToggleDelivery() {
    constexpr int kBefore = 10;
    constexpr int kAfter = 30;
    const std::vector<ChargedAtomSpec> specs = chargedSpecs();

    Simulation cpu;
    configureSim(cpu, /*lj=*/false, /*coulomb=*/true);
    fillCharged(cpu, specs);
    Simulation gpu;
    configureSim(gpu, /*lj=*/false, /*coulomb=*/true);
    fillCharged(gpu, specs);
    gpu.setGpuMode(true);
    Simulation cpuStale; // эталон: Coulomb НЕ выключаем
    configureSim(cpuStale, /*lj=*/false, /*coulomb=*/true);
    fillCharged(cpuStale, specs);

    for (int s = 0; s < kBefore; ++s) {
        cpu.updateAll();
        gpu.updateAll();
        cpuStale.updateAll();
    }
    cpu.setCoulombEnabled(false);
    gpu.setCoulombEnabled(false); // с фиксом: бампит версию → ближайший update re-upload'ит coulombEnabled_=false
    for (int s = 0; s < kAfter; ++s) {
        cpu.updateAll();
        gpu.updateAll();
        cpuStale.updateAll();
    }
    gpu.syncFromGpuIfNeeded();

    CoulombToggleResult r{};
    r.parity = maxAbsPositionDiff(cpu.atoms(), gpu.atoms());
    r.staleGapRef = maxAbsPositionDiff(cpu.atoms(), cpuStale.atoms());
    std::printf("[ COULPAR  ] coulomb-toggle: before=%d after=%d max|dCPU-GPU|=%.3e abs (stale-gap ref=%.3e)\n",
                kBefore, kAfter, r.parity, r.staleGapRef);
    return r;
}

void runCoulombParityGate(benchmark::State& state) {
    benchmarkDevice();

    for (auto _ : state) {
        const CombinedResult comb = runCombined();
        const CoulombOnlyResult only = runCoulombOnly();
        const UnchargedResult unch = runUncharged();
        const CoulombToggleResult tog = runCoulombToggleDelivery();

        state.counters["combined_max_abs"] = comb.trajMaxAbs;
        state.counters["combined_cpu_self_disp"] = comb.cpuSelfDisplacement;
        state.counters["combined_pe_gpu"] = comb.gpuPeSum;
        state.counters["combined_pe_cpu"] = comb.cpuPeSum;
        state.counters["combined_pe_absdiff"] = comb.peAbsDiff;
        state.counters["combined_lj_pe_signal"] = comb.ljPeSignal;
        state.counters["coulomb_only_max_abs"] = only.trajMaxAbs;
        state.counters["uncharged_max_abs"] = unch.trajMaxAbs;
        state.counters["coulomb_toggle_parity"] = tog.parity;
        state.counters["coulomb_toggle_stale_gap"] = tog.staleGapRef;

        // (e) Заряды доставлены в VRAM (резидентные charges_ == CPU). Без этого GPU
        // Coulomb = 0 → ложный pass (как устаревшая gravity / пустые bonds).
        if (!comb.chargesMatch) {
            throw std::runtime_error("CoulombParity: resident charges != CPU charges (charges not delivered to VRAM)");
        }

        // (c-blind) CPU реально двинулся → силы НЕнулевы (комбинированный И coulomb-only).
        // Без этого 0==0 «прошло» бы слепо (charge-gated сила слепа без guard'а).
        constexpr double kNonZeroDisp = 1e-4;
        if (comb.cpuSelfDisplacement <= kNonZeroDisp) {
            throw std::runtime_error("CoulombParity: combined CPU produced ~0 displacement — gate is blind (force is zero)");
        }
        if (only.cpuSelfDisplacement <= kNonZeroDisp) {
            throw std::runtime_error("CoulombParity: coulomb-only CPU produced ~0 displacement — gate is blind (force is zero)");
        }
        // (d-blind) uncharged-сцена тоже должна реально двигаться (LJ активна), иначе
        // «Coulomb adds 0» проверялось бы на неподвижной сцене (слепо).
        if (unch.cpuSelfDisplacement <= kNonZeroDisp) {
            throw std::runtime_error("CoulombParity: uncharged CPU produced ~0 displacement — no-regression check is blind");
        }

        // (c) GPU vs CPU траектория. Tolerance: Coulomb весь во float (CoulombForceField.h:27-40),
        // GPU всё в f32 → дрейф уровня LJ (нет double-геометрии как у angle). Только
        // sqrt-форма (1.0/sqrt, как CPU) + порядок суммирования. Порог ПО ИЗМЕРЕНИЮ,
        // провенанс в RESULTS.md. Стартовая гипотеза ~1e-2 (уровень BM_GpuCorrectness).
        constexpr double kTrajTol = 1e-2;
        if (only.trajMaxAbs > kTrajTol) {
            throw std::runtime_error("GPU Coulomb-only trajectory diverged from CPU beyond tolerance");
        }
        if (comb.trajMaxAbs > kTrajTol) {
            throw std::runtime_error("GPU combined LJ+Coulomb trajectory diverged from CPU beyond tolerance");
        }
        // (d) uncharged: Coulomb добавил ровно 0 → GPU == CPU В ТОМ ЖЕ ПОРОГЕ, что
        // LJ-only (BM_GpuCorrectness). Если Coulomb на charge-0 что-то добавил бы —
        // траектория разошлась бы. Тот же kTrajTol (фактически LJ-only паритет).
        if (unch.trajMaxAbs > kTrajTol) {
            throw std::runtime_error("CoulombParity: uncharged scene diverged — Coulomb added non-zero on charge-0 atoms");
        }

        // (a) Комбинированный PE-assert (ловит .w SET-вместо-ADD). Резидентный .w несёт
        // LJ_PE+Coulomb_PE; CPU atoms.energy() — то же. Если бы compute_coulomb писал
        // = pe (SET) вместо prev.w+pe (ADD), GPU .w нёс бы ТОЛЬКО Coulomb_PE (LJ_PE
        // затёрт) → absdiff ≈ |LJ_PE|. Порог ОБЯЗАН быть НИЖЕ |LJ_PE|, иначе SET
        // проскользнёт: |LJ_PE|/|полная PE| ≈ 6e-4 (LJ_PE мал на фоне Coulomb), а
        // относительный порог по полной PE был бы крупнее этого сигнала → слеп. Поэтому
        // порог привязан к ИЗМЕРЕННОМУ LJ_PE-сигналу (comb.ljPeSignal): абсолютный
        // 0.25·|LJ_PE|. SET → absdiff ≈ |LJ_PE| > 0.25·|LJ_PE| → ПРОВАЛ. Легитимный ADD →
        // absdiff ≈ 0 (бит-в-бит) → пройдёт. Форма !(... <) ловит и NaN.

        // (a-blind) Должен БЫТЬ значимый LJ_PE-сигнал, который SET затёр бы. Если LJ_PE≈0
        // (атомы за cutoff / сцена разлетелась), assert вырождается в ~0 порог и слеп к
        // SET — падаем громко, чтобы мейнтейнер усилил сцену (как 0==0-вырождение).
        constexpr double kLjSignalMin = 5e-3;
        if (!(std::abs(comb.ljPeSignal) > kLjSignalMin)) {
            std::printf("[ COULPAR  ] LJ_PE-signal=%.6e <= %.1e — нет сигнала для PE-assert, гейт слеп к .w "
                        "SET-вместо-ADD; усилить сцену (атомы ближе в пределах cutoff)\n",
                        comb.ljPeSignal, kLjSignalMin);
            std::fflush(stdout); // диагностика должна долететь до abort при необработанном throw
            throw std::runtime_error("CoulombParity: LJ_PE signal ~0 — PE-assert cannot catch .w SET (scene must be strengthened)");
        }

        const double kPeAbsTol = 0.25 * std::abs(comb.ljPeSignal);
        if (!(comb.peAbsDiff < kPeAbsTol)) {
            std::printf("[ COULPAR  ] PE-assert FAIL: gpu=%.6e cpu=%.6e absdiff=%.3e (порог %.3e = 0.25·|LJ_PE| "
                        "при LJ_PE-signal=%.6e) — .w SET-вместо-ADD? kernel ОБЯЗАН прибавлять prev.w+pe\n",
                        comb.gpuPeSum, comb.cpuPeSum, comb.peAbsDiff, kPeAbsTol, comb.ljPeSignal);
            std::fflush(stdout); // диагностика должна долететь до abort при необработанном throw
            throw std::runtime_error("CoulombParity: combined LJ+Coulomb PE (.w) diverged — likely .w SET instead of ADD");
        }

        // (b) Рантайм-смена setCoulombEnabled долетела до GPU (фикс: бампит версию).
        // Доставлено ⟺ GPU(Coulomb выкл) НАМНОГО ближе к CPU(выкл), чем масштаб самого
        // эффекта toggle (staleGapRef). Если бы не долетела, GPU держал бы Coulomb →
        // parity ≈ staleGapRef. Порог ОТНОСИТЕЛЬНЫЙ. Форма !(parity < ...) ловит NaN.
        if (!(tog.parity < 0.25 * tog.staleGapRef)) {
            throw std::runtime_error("GPU did not honor runtime setCoulombEnabled(false) — parity not << toggle effect");
        }
        // Тест не слеп: выключение Coulomb реально изменило траекторию заметно выше шума.
        if (tog.staleGapRef < 5e-4) {
            throw std::runtime_error("Coulomb toggle had no measurable effect — test is blind (Coulomb inactive over run)");
        }
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// @bench_meta {"id":"GpuCoulombParity/CoulombMatchesCpu","ru":"GPU Coulomb == CPU","group":"Симуляция/GPU"}
void BM_GpuCoulombParity_CoulombMatchesCpu(benchmark::State& state) { runCoulombParityGate(state); }

BENCHMARK(BM_GpuCoulombParity_CoulombMatchesCpu)->Unit(benchmark::kMillisecond);
