#pragma once

#include "Plugins/LatticeParticleAPI/ParticleAPI.hpp"

class ClassicMD {
public:
    void setIntegrator(IntegratorAPI* api) {
        integrator = api;
    }

    void step(float dt) {
        if (integrator)
            integrator->step(integrator->instance, dt);
    }

private:
    IntegratorAPI* integrator = nullptr;
};