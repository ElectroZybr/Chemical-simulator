#include "VerletScheme.h"

#include "Engine/metrics/Profiler.h"
#include "Engine/physics/integrators/StepOps.h"

#ifdef ENABLE_TBB
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#endif

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

#ifdef ENABLE_TBB
    tbb::parallel_for(tbb::blocked_range<size_t>(0, n),
        [&](const tbb::blocked_range<size_t>& r) {
            const size_t begin = r.begin();
            float* __restrict__ lx = x + begin;
            float* __restrict__ ly = y + begin;
            float* __restrict__ lz = z + begin;
            const float* __restrict__ lvx = vx + begin;
            const float* __restrict__ lvy = vy + begin;
            const float* __restrict__ lvz = vz + begin;
            const float* __restrict__ lfx = fx + begin;
            const float* __restrict__ lfy = fy + begin;
            const float* __restrict__ lfz = fz + begin;
            const float* __restrict__ lim = invMass + begin;
            const size_t len = r.end() - begin;
#pragma GCC ivdep
            for (size_t i = 0; i < len; ++i) {
                lx[i] += (lvx[i] + lfx[i] * lim[i] * 0.5f * dt) * dt;
                ly[i] += (lvy[i] + lfy[i] * lim[i] * 0.5f * dt) * dt;
                lz[i] += (lvz[i] + lfz[i] * lim[i] * 0.5f * dt) * dt;
            }
        });
#else
#pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        x[i] += (vx[i] + fx[i] * invMass[i] * 0.5f * dt) * dt;
        y[i] += (vy[i] + fy[i] * invMass[i] * 0.5f * dt) * dt;
        z[i] += (vz[i] + fz[i] * invMass[i] * 0.5f * dt) * dt;
    }
#endif
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

    const float halfDt = 0.5f * accelDamping * dt;
#ifdef ENABLE_TBB
    tbb::parallel_for(tbb::blocked_range<size_t>(0, n),
        [&](const tbb::blocked_range<size_t>& r) {
            const size_t begin = r.begin();
            const float* __restrict__ lfx = fx + begin;
            const float* __restrict__ lfy = fy + begin;
            const float* __restrict__ lfz = fz + begin;
            const float* __restrict__ lpfx = pfx + begin;
            const float* __restrict__ lpfy = pfy + begin;
            const float* __restrict__ lpfz = pfz + begin;
            float* __restrict__ lvx = vx + begin;
            float* __restrict__ lvy = vy + begin;
            float* __restrict__ lvz = vz + begin;
            const float* __restrict__ lim = invMass + begin;
            const size_t len = r.end() - begin;
#pragma GCC ivdep
            for (size_t i = 0; i < len; ++i) {
                const float halfDtInvMass = halfDt * lim[i];
                lvx[i] += (lpfx[i] + lfx[i]) * halfDtInvMass;
                lvy[i] += (lpfy[i] + lfy[i]) * halfDtInvMass;
                lvz[i] += (lpfz[i] + lfz[i]) * halfDtInvMass;
            }
        });
#else
#pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        const float halfDtInvMass = halfDt * invMass[i];
        vx[i] += (pfx[i] + fx[i]) * halfDtInvMass;
        vy[i] += (pfy[i] + fy[i]) * halfDtInvMass;
        vz[i] += (pfz[i] + fz[i]) * halfDtInvMass;
    }
#endif
}
