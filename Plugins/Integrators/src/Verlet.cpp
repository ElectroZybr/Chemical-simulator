#include "Verlet.hpp"

// #include "src/StepOps.hpp"
#include "Lattice/Engine/restrict.h"

namespace Integrators {

void Verlet::pipeline() {
    // Расчет новых позиций
    predict();
    // // Расчет сил
    // StepOps::computeForces();
    // Корректировка скоростей
    correct();
    // StepOps::applyThermostat();
    // StepOps::postProcessVelocities();
}

void Verlet::predict() {
    const size_t n = particles->mobileCount();

    float* RESTRICT x = particles->getCol<Pos::X>();
    float* RESTRICT y = particles->getCol<Pos::Y>();
    float* RESTRICT z = particles->getCol<Pos::Z>();

    const float* RESTRICT vx = particles->getCol<Vel::X>();
    const float* RESTRICT vy = particles->getCol<Vel::Y>();
    const float* RESTRICT vz = particles->getCol<Vel::Z>();

    const float* RESTRICT fx = particles->getCol<Force::X>();
    const float* RESTRICT fy = particles->getCol<Force::Y>();
    const float* RESTRICT fz = particles->getCol<Force::Z>();

    const float* RESTRICT invMass = particles->getCol<InvMass>();

    #pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        x[i] += (vx[i] + fx[i] * invMass[i] * 0.5f * dt) * dt;
    }
    #pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        y[i] += (vy[i] + fy[i] * invMass[i] * 0.5f * dt) * dt;
    }
    #pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        z[i] += (vz[i] + fz[i] * invMass[i] * 0.5f * dt) * dt;
    }

    Log::ok("Verlet", "step dt: {}", dt);
}

void Verlet::correct() {
    const size_t n = particles->mobileCount();

    float* RESTRICT vx = particles->getCol<Vel::X>();
    float* RESTRICT vy = particles->getCol<Vel::Y>();
    float* RESTRICT vz = particles->getCol<Vel::Z>();

    const float* RESTRICT fx = particles->getCol<Force::X>();
    const float* RESTRICT fy = particles->getCol<Force::Y>();
    const float* RESTRICT fz = particles->getCol<Force::Z>();

    const float* RESTRICT pfx = particles->getCol<PrevForceX>();
    const float* RESTRICT pfy = particles->getCol<PrevForceY>();
    const float* RESTRICT pfz = particles->getCol<PrevForceZ>();

    const float* RESTRICT invMass = particles->getCol<InvMass>();

    #pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        const float halfDtInvMass = 0.5f * dt * invMass[i];
        vx[i] += (pfx[i] + fx[i]) * halfDtInvMass;
    }
    #pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        const float halfDtInvMass = 0.5f * dt * invMass[i];
        vy[i] += (pfy[i] + fy[i]) * halfDtInvMass;
    }
    #pragma GCC ivdep
    for (size_t i = 0; i < n; ++i) {
        const float halfDtInvMass = 0.5f * dt * invMass[i];
        vz[i] += (pfz[i] + fz[i]) * halfDtInvMass;
    }
}
}