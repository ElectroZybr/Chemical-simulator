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

    for (size_t i = 0; i < mobileCount; ++i) {
        vx[i] += 0.5f * fx[i] * invMass[i] * accelDamping * dt;
        vy[i] += 0.5f * fy[i] * invMass[i] * accelDamping * dt;
        vz[i] += 0.5f * fz[i] * invMass[i] * accelDamping * dt;
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
    for (size_t i = 0; i < mobileCount; ++i) {
        x[i] += vx[i] * dt;
        y[i] += vy[i] * dt;
        z[i] += vz[i] * dt;
    }
}
