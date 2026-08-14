// #include "KDK.h"

// #include "Lattice/Engine/metrics/Profiler.h"
// #include "Integrators/StepOps.h"

// void KDK::pipeline(StepContext& stepContext) const {
//     PROFILE_SCOPE("KDK::pipeline");
//     halfKick(stepContext.world.getAtomStorage(), stepContext.dt);
//     StepOps::predictAndSync(stepContext, &KDK::drift);
//     StepOps::computeForces(stepContext);
//     halfKick(stepContext.world.getAtomStorage(), stepContext.dt);
//     StepOps::applyThermostat(stepContext);
//     StepOps::postProcessVelocities(stepContext);
// }

// void KDK::halfKick(AtomStorage& atomStorage, float dt) {
//     PROFILE_SCOPE("KDK::halfKick");
//     const float* RESTRICT fx = atomStorage.fx().data();
//     const float* RESTRICT fy = atomStorage.fy().data();
//     const float* RESTRICT fz = atomStorage.fz().data();

//     float* RESTRICT vx = atomStorage.vx().data();
//     float* RESTRICT vy = atomStorage.vy().data();
//     float* RESTRICT vz = atomStorage.vz().data();

//     const float* RESTRICT invMass = atomStorage.invMass().data();
//     const size_t mobileCount = atomStorage.mobileCount();

//     #pragma GCC ivdep
//     for (size_t i = 0; i < mobileCount; ++i) {
//         vx[i] += 0.5f * fx[i] * invMass[i] * dt;
//     }
//     #pragma GCC ivdep
//     for (size_t i = 0; i < mobileCount; ++i) {
//         vy[i] += 0.5f * fy[i] * invMass[i] * dt;
//     }
//     #pragma GCC ivdep
//     for (size_t i = 0; i < mobileCount; ++i) {
//         vz[i] += 0.5f * fz[i] * invMass[i] * dt;
//     }
// }

// void KDK::drift(AtomStorage& atomStorage, float dt) {
//     PROFILE_SCOPE("KDK::drift");
//     float* RESTRICT x = atomStorage.x().data();
//     float* RESTRICT y = atomStorage.y().data();
//     float* RESTRICT z = atomStorage.z().data();

//     const float* RESTRICT vx = atomStorage.vx().data();
//     const float* RESTRICT vy = atomStorage.vy().data();
//     const float* RESTRICT vz = atomStorage.vz().data();

//     const size_t mobileCount = atomStorage.mobileCount();
//     #pragma GCC ivdep
//     for (size_t i = 0; i < mobileCount; ++i) {
//         x[i] += vx[i] * dt;
//     }
//     #pragma GCC ivdep
//     for (size_t i = 0; i < mobileCount; ++i) {
//         y[i] += vy[i] * dt;
//     }
//     #pragma GCC ivdep
//     for (size_t i = 0; i < mobileCount; ++i) {
//         z[i] += vz[i] * dt;
//     }
// }
