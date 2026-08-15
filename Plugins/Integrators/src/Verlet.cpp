// #include "Verlet.hpp"

// #include "src/StepOps.hpp"

// namespace Integrators {

// void Verlet::pipeline() const {
//     // Расчет новых позиций
//     StepOps::predict();
//     // Расчет сил
//     StepOps::computeForces();
//     // Корректировка скоростей
//     Verlet::correct();
//     // StepOps::applyThermostat();
//     // StepOps::postProcessVelocities();
// }

// void Verlet::predict() {
//     const size_t n = atomStorage.mobileCount();

//     float* RESTRICT x = atomStorage.x().data();
//     float* RESTRICT y = atomStorage.y().data();
//     float* RESTRICT z = atomStorage.z().data();

//     const float* RESTRICT fx = atomStorage.fx().data();
//     const float* RESTRICT fy = atomStorage.fy().data();
//     const float* RESTRICT fz = atomStorage.fz().data();

//     const float* RESTRICT vx = atomStorage.vx().data();
//     const float* RESTRICT vy = atomStorage.vy().data();
//     const float* RESTRICT vz = atomStorage.vz().data();

//     const float* RESTRICT invMass = atomStorage.invMass().data();

//     #pragma GCC ivdep
//     for (size_t i = 0; i < n; ++i) {
//         x[i] += (vx[i] + fx[i] * invMass[i] * 0.5f * dt) * dt;
//     }
//     #pragma GCC ivdep
//     for (size_t i = 0; i < n; ++i) {
//         y[i] += (vy[i] + fy[i] * invMass[i] * 0.5f * dt) * dt;
//     }
//     #pragma GCC ivdep
//     for (size_t i = 0; i < n; ++i) {
//         z[i] += (vz[i] + fz[i] * invMass[i] * 0.5f * dt) * dt;
//     }
// }

// void Verlet::correct() {
//     const size_t n = atomStorage.mobileCount();

//     const float* RESTRICT fx = atomStorage.fx().data();
//     const float* RESTRICT fy = atomStorage.fy().data();
//     const float* RESTRICT fz = atomStorage.fz().data();

//     const float* RESTRICT pfx = atomStorage.pfx().data();
//     const float* RESTRICT pfy = atomStorage.pfy().data();
//     const float* RESTRICT pfz = atomStorage.pfz().data();

//     float* RESTRICT vx = atomStorage.vx().data();
//     float* RESTRICT vy = atomStorage.vy().data();
//     float* RESTRICT vz = atomStorage.vz().data();

//     const float* RESTRICT invMass = atomStorage.invMass().data();

//     #pragma GCC ivdep
//     for (size_t i = 0; i < n; ++i) {
//         const float halfDtInvMass = 0.5f * dt * invMass[i];
//         vx[i] += (pfx[i] + fx[i]) * halfDtInvMass;
//     }
//     #pragma GCC ivdep
//     for (size_t i = 0; i < n; ++i) {
//         const float halfDtInvMass = 0.5f * dt * invMass[i];
//         vy[i] += (pfy[i] + fy[i]) * halfDtInvMass;
//     }
//     #pragma GCC ivdep
//     for (size_t i = 0; i < n; ++i) {
//         const float halfDtInvMass = 0.5f * dt * invMass[i];
//         vz[i] += (pfz[i] + fz[i]) * halfDtInvMass;
//     }
// }
// }