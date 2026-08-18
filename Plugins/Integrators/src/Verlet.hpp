#pragma once

#include <string>

#include <Lattice/Tools/Logger.hpp>

#include <Plugins/ParticleDynamics/api/ParticleAPI.hpp>
#include <Plugins/ParticleDynamics/api/ParticleStorage.hpp>

#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

namespace Integrators {

class Verlet final : public ParticleDynamics::IntegratorAPI{
public:
    struct PrevForceX {using type = float;};
    struct PrevForceY {using type = float;};
    struct PrevForceZ {using type = float;};

    Verlet(Lattice::Components& components) 
        // интегратор требует для работы буфер. Если нет - исключение
        : particles(components.require<ParticleDynamics::ParticleStorage>())
    {
        // дополнительные поля для работы интегратора
        particles->addCol<PrevForceX>();
        particles->addCol<PrevForceY>();
        particles->addCol<PrevForceZ>();

        Lattice::Component settings = components.require<Lattice::Settings>();
        settings->bind("verlet", "dt", &dt, 0, 0.1, true);
    }

    void step() override { pipeline(); }

    ~Verlet () {
        Logger::info("Verlet", "destroying object");
        particles->removeCol<PrevForceX>();
        particles->removeCol<PrevForceY>();
        particles->removeCol<PrevForceZ>();
    }

private:
    void pipeline();
    void predict();
    void correct();

    float dt = 0.01;

    Lattice::Component<ParticleDynamics::ParticleStorage> particles;
};
}