#pragma once

#include <glm/vec3.hpp>

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ModelAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences
#include <ParticleDynamics/api/ParticleAPI.hpp>
#include <ParticleDynamics/api/ParticleStorage.hpp>
#include <ParticleDynamics/src/SpatialGrid.hpp>

#include <Integrators/src/Verlet.hpp>

// Source
// #include "AtomStorage.hpp"
#include <Lattice/Engine/physics/Atom/AtomData.h>

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
        integrator = universe.addInterfaceSlot<ParticleDynamics::IntegratorAPI>();
        atoms = universe.addComponent<ParticleDynamics::ParticleStorage>();
        spatialGrid = universe.addComponent<ParticleDynamics::SpatialGrid>();
        // universe.useInterface<ParticleDynamics::IntegratorAPI>("Verlet");

        universe.addComponent<Integrators::Verlet>();
    }

    void configure() {
        atoms->addCol<Energy>();
        atoms->addCol<Charge>();
        atoms->addCol<Type>();
        atoms->addCol<Valence>();
        atoms->addCol<Hybridization>();
        atoms->addCol<Id>();
    }

    void update() override {
        integrator->step();
        settings->set("SpatialGrid", "size", glm::vec3(10, 10, 10));
    }

    ~ClassicMD() {
        // atoms->removeCol<Energy>();
        // atoms->removeCol<Charge>();
        // atoms->removeCol<Type>();
        // atoms->removeCol<Valence>();
        // atoms->removeCol<Hybridization>();
        // atoms->removeCol<Id>();
        Logger::info("ClassicMD", "destroying object");
    }

private:
    Lattice::Component<Lattice::Settings> settings;
    Lattice::Component<ParticleDynamics::IntegratorAPI> integrator;
    Lattice::Component<ParticleDynamics::ParticleStorage> atoms;
    Lattice::Component<ParticleDynamics::SpatialGrid> spatialGrid;
};

} // namespace ClassicMD