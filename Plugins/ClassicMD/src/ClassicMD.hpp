#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ModelAPI.hpp>
#include <Lattice/Kernel/Component.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences
#include <Plugins/ParticleDynamics/src/ParticleAPI.hpp>
#include <Plugins/ParticleDynamics/src/ParticleStorage.hpp>

// Source
// #include "AtomStorage.hpp"

namespace ClassicMD {

class ClassicMD final : public ModelAPI {
public:
    struct Energy {using type = float;};
    struct Charge {using type = float;};

    struct Type {using type = AtomData::Type;};
    struct Valence {using type = uint8_t;};
    struct Hybridization {using type = AtomData::Hybridization;};
    struct Id {using type = uint32_t;};

    explicit ClassicMD(Lattice::Components& universe) {
        settings = universe.require<Lattice::Settings>();
        integrator = universe.add<ParticleDynamics::IntegratorAPI>();
        atoms = universe.add<ParticleDynamics::ParticleStorage>();

        atoms->addCol<Energy>();
        atoms->addCol<Charge>();
        atoms->addCol<Type>();
        atoms->addCol<Valence>();
        atoms->addCol<Hybridization>();
        atoms->addCol<Id>();

        universe.use<ParticleDynamics::IntegratorAPI>("Verlet");
    }

    void update() override {
        integrator->step();
        // atoms->get<PosX>();
        // atoms->at<PosX>(0);
    }

    ~ClassicMD() {
        atoms->removeCol<Energy>();
        atoms->removeCol<Charge>();
        atoms->removeCol<Type>();
        atoms->removeCol<Valence>();
        atoms->removeCol<Hybridization>();
        atoms->removeCol<Id>();
        Log::info("ClassicMD", "destroying object");
    }

private:
    // Lattice::Component<Lattice::Components> universe;
    Lattice::Component<Lattice::Settings> settings;
    Lattice::Component<ParticleDynamics::IntegratorAPI> integrator;
    Lattice::Component<ParticleDynamics::ParticleStorage> atoms;
};

} // namespace ClassicMD