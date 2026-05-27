#include "VerletScheme.h"

#include "Engine/metrics/Profiler.h"
#include "Engine/physics/integrators/StepOps.h"

void VerletScheme::pipeline(StepData& stepData) const {
    PROFILE_SCOPE("VerletScheme::pipeline");
    // Расчет новых позиций
    StepOps::predictAndSync(stepData, &predict);
    // Расчет сил
    StepOps::computeForces(stepData);
    // Корректировка скоростей
    correct(stepData.world.getAtomStorage(), stepData.accelDamping, stepData.dt);
}

void VerletScheme::predict(AtomStorage& atomStorage, float dt) {
    const size_t n = atomStorage.mobileCount();

    float* RESTRICT x = atomStorage.xData();
    float* RESTRICT y = atomStorage.yData();
    float* RESTRICT z = atomStorage.zData();

    const float* RESTRICT fx = atomStorage.fxData();
    const float* RESTRICT fy = atomStorage.fyData();
    const float* RESTRICT fz = atomStorage.fzData();

    const float* RESTRICT vx = atomStorage.vxData();
    const float* RESTRICT vy = atomStorage.vyData();
    const float* RESTRICT vz = atomStorage.vzData();

    const float* RESTRICT invMass = atomStorage.invMassData();

#pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        x[i] += (vx[i] + fx[i] * invMass[i] * 0.5f * dt) * dt;
        y[i] += (vy[i] + fy[i] * invMass[i] * 0.5f * dt) * dt;
        z[i] += (vz[i] + fz[i] * invMass[i] * 0.5f * dt) * dt;
    }
}

void VerletScheme::correct(AtomStorage& atomStorage, float accelDamping, float dt) {
    PROFILE_SCOPE("VerletScheme::correct");
    const size_t n = atomStorage.mobileCount();

    const float* RESTRICT fx = atomStorage.fxData();
    const float* RESTRICT fy = atomStorage.fyData();
    const float* RESTRICT fz = atomStorage.fzData();

    const float* RESTRICT pfx = atomStorage.pfxData();
    const float* RESTRICT pfy = atomStorage.pfyData();
    const float* RESTRICT pfz = atomStorage.pfzData();

    float* RESTRICT vx = atomStorage.vxData();
    float* RESTRICT vy = atomStorage.vyData();
    float* RESTRICT vz = atomStorage.vzData();

    const float* RESTRICT invMass = atomStorage.invMassData();

#pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        const float halfDtInvMass = 0.5f * accelDamping * dt * invMass[i];

        vx[i] += (pfx[i] + fx[i]) * halfDtInvMass;
        vy[i] += (pfy[i] + fy[i]) * halfDtInvMass;
        vz[i] += (pfz[i] + fz[i]) * halfDtInvMass;
    }
}
