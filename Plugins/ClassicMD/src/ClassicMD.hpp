#pragma once

#include <glm/vec3.hpp>

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences
#include <ParticleDynamics/api/ParticleAPI.hpp>
#include <ParticleDynamics/api/ParticleStorage.hpp>

// Source
// #include "AtomStorage.hpp"
#include <Lattice/Engine/physics/Atom/AtomData.h>

namespace ClassicMD {

class ClassicMD final : public ServiceAPI {
public:
    struct Energy {using type = float;};
    struct Charge {using type = float;};

    struct Type {using type = AtomData::Type;};
    struct Valence {using type = uint8_t;};
    struct Hybridization {using type = AtomData::Hybridization;};
    struct Id {using type = uint32_t;};

    explicit ClassicMD(Lattice::Components& universe) {
        settings = universe.require<Lattice::Settings>();
        atoms = universe.add<ParticleDynamics::ParticleStorage>();
        spatialGrid = universe.use<ParticleDynamics::SpatialIndexAPI>("SpatialGrid");
        integrator = universe.use<ParticleDynamics::IntegratorAPI>("Verlet");

        atoms->addCol<Energy>();
        atoms->addCol<Charge>();
        atoms->addCol<Type>();
        atoms->addCol<Valence>();
        atoms->addCol<Hybridization>();
        atoms->addCol<Id>();
    }

    void configure() {

    }

    void run() override {
        while (!stopRequested()) {
            Logger::info("ClassicMD", "looping");
            integrator->step();
            settings->set("SpatialGrid", "size", glm::vec3(10, 10, 10));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    ~ClassicMD() {
        if (atoms) {
            atoms->removeCol<Energy>();
            atoms->removeCol<Charge>();
            atoms->removeCol<Type>();
            atoms->removeCol<Valence>();
            atoms->removeCol<Hybridization>();
            atoms->removeCol<Id>();
        }
        Logger::info("ClassicMD", "destroying object");
    }

private:
    Lattice::Settings* settings = nullptr;
    Lattice::Slot<ParticleDynamics::IntegratorAPI> integrator;
    ParticleDynamics::ParticleStorage* atoms = nullptr;
    Lattice::Slot<ParticleDynamics::SpatialIndexAPI> spatialGrid;
};

} // namespace ClassicMD