#include "KDKScheme.h"

#include "Engine/metrics/Profiler.h"
#include "Engine/physics/integrators/StepOps.h"

void KDKScheme::pipeline(StepData& stepData) const {
    PROFILE_SCOPE("KDKScheme::pipeline");
    // Kick: половина шага
    halfKick(stepData.world.getAtomStorage(), stepData.accelDamping, stepData.dt);
    // Расчет новых позиций
    StepOps::predictAndSync(stepData, &drift);
    // Расчет сил
    StepOps::computeForces(stepData);
    // Kick: вторая половина шага
    halfKick(stepData.world.getAtomStorage(), stepData.accelDamping, stepData.dt);
}

void KDKScheme::halfKick(AtomStorage& atomStorage, float accelDamping, float dt) {
    PROFILE_SCOPE("KDKScheme::halfKick");
    const float* RESTRICT fx = atomStorage.fxData();
    const float* RESTRICT fy = atomStorage.fyData();
    const float* RESTRICT fz = atomStorage.fzData();

    float* RESTRICT vx = atomStorage.vxData();
    float* RESTRICT vy = atomStorage.vyData();
    float* RESTRICT vz = atomStorage.vzData();

    const float* RESTRICT invMass = atomStorage.invMassData();

    const size_t mobileCount = atomStorage.mobileCount();

    const float halfDt = 0.5f * accelDamping * dt;
#pragma GCC ivdep
    for (size_t i = 0; i < mobileCount; ++i) {
        const float halfDtInvMass = halfDt * invMass[i];
        vx[i] += fx[i] * halfDtInvMass;
        vy[i] += fy[i] * halfDtInvMass;
        vz[i] += fz[i] * halfDtInvMass;
    }
}

void KDKScheme::drift(AtomStorage& atomStorage, float dt) {
    PROFILE_SCOPE("KDKScheme::drift");
    float* RESTRICT x = atomStorage.xData();
    float* RESTRICT y = atomStorage.yData();
    float* RESTRICT z = atomStorage.zData();

    const float* RESTRICT vx = atomStorage.vxData();
    const float* RESTRICT vy = atomStorage.vyData();
    const float* RESTRICT vz = atomStorage.vzData();

    const size_t mobileCount = atomStorage.mobileCount();
#pragma GCC ivdep
    for (size_t i = 0; i < mobileCount; ++i) {
        x[i] += vx[i] * dt;
        y[i] += vy[i] * dt;
        z[i] += vz[i] * dt;
    }
}
