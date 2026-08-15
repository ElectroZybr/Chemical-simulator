#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ModelAPI.hpp>
#include <Lattice/Kernel/Component.hpp>

// Plugin dependences
#include <Plugins/ParticleDynamics/src/ParticleAPI.hpp>
#include <Plugins/ParticleDynamics/src/DynamicSoALib.hpp>

// Source
#include "AtomStorage.hpp"

namespace ClassicMD {

class ClassicMD final : public ModelAPI {
public:
    explicit ClassicMD(Lattice::Components& components)
        : universe(components.add<Lattice::Components>("universe"))
    {
        integrator = universe->add<ParticleDynamics::IntegratorAPI>();
        atoms = universe->add<AtomStorage>();
        universe->use<ParticleDynamics::IntegratorAPI>("Verlet");
        Log::ok("ClassicMD", "Configure done");
    }

    void update() override {
        integrator->step(0.01f);
        // atoms->get<PosX>();
        // atoms->at<PosX>(0);
    }

    ~ClassicMD() {

    }

private:
    Lattice::Component<Lattice::Components> universe;
    Lattice::Component<ParticleDynamics::IntegratorAPI> integrator;
    Lattice::Component<AtomStorage> atoms;
};

} // namespace ClassicMD