#pragma once

#include <Lattice/Tools/Logger.hpp>

#include <Plugins/ParticleDynamics/api/ParticleAPI.hpp>
#include <Plugins/ParticleDynamics/api/ParticleStorage.hpp>

#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

namespace Integrators {

class Verlet final : public ParticleDynamics::IntegratorAPI {
public:
    struct PrevForceX {using type = float;};
    struct PrevForceY {using type = float;};
    struct PrevForceZ {using type = float;};

    Verlet(Lattice::Components& components) 
        // интегратор требует для работы буфер. Если нет - исключение
        : particles(components.require<ParticleDynamics::ParticleStorage>())
        , settings(components.require<Lattice::Settings>())
    {
        settings->bind("verlet", "dt", &dt, 0, 0.1, true);
    }

    void configure() {
        particles->addCol<PrevForceX>();
        particles->addCol<PrevForceY>();
        particles->addCol<PrevForceZ>();
        configured = true;
    }

    void step() override;

    ~Verlet () {
        if (settings)
            settings->unbind("verlet", "dt");
        if (configured && particles) {
            particles->removeCol<PrevForceX>();
            particles->removeCol<PrevForceY>();
            particles->removeCol<PrevForceZ>();
        }
        Logger::info("Verlet", "destroying object");
    }

private:
    void predict();
    void correct();

    float dt = 0.01;
    bool configured = false;

    ParticleDynamics::ParticleStorage* particles = nullptr;
    Lattice::Settings* settings = nullptr;
};
}