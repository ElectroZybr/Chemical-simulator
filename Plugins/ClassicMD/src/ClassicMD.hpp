#pragma once

#include <glm/vec3.hpp>

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences
#include <ParticleDynamics/include/ParticleAPI.hpp>
#include <ParticleDynamics/include/ParticleStorage.hpp>

// Source
// #include "AtomStorage.hpp"
#include <Lattice/Engine/physics/Atom/AtomData.h>
#include "StdData/include/SoA.hpp"

namespace ClassicMD {

class ClassicMD final : public ServiceAPI {
public:
    struct Energy {using type = float;};
    struct Charge {using type = float;};

    struct Type {using type = AtomData::Type;};
    struct Valence {using type = uint8_t;};
    struct Hybridization {using type = AtomData::Hybridization;};
    struct Id {using type = uint32_t;};

    struct Element {using type = float;};
    struct Mass {using type = float;};

    explicit ClassicMD(Lattice::Components& universe) {
        universe.add<StdData::SoA>("atomData");
        universe.add<ParticleDynamics::ParticleStorage>();
        universe.use<ParticleDynamics::SpatialIndexAPI>("SpatialGrid");
        universe.use<ParticleDynamics::IntegratorAPI>("Verlet");
    }

    void configure(Lattice::Components& universe) {
        settings = universe.require<Lattice::Settings>();
        atomData = universe.require<StdData::SoA>("atomData");
        atoms = universe.require<ParticleDynamics::ParticleStorage>();
        spatialGrid = universe.find<ParticleDynamics::SpatialIndexAPI>();
        integrator = universe.find<ParticleDynamics::IntegratorAPI>();
        atoms->addCol<Energy>();
        atoms->addCol<Charge>();
        atoms->addCol<Type>();
        atoms->addCol<Valence>();
        atoms->addCol<Hybridization>();
        atoms->addCol<Id>();

        atomData->addCol<Element>();
        atomData->addCol<Mass>();
        atomData->addCol<Valence>();
    }

    void run() override {
        while (!stopRequested()) {
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
    Ref<StdData::SoA> atomData;
    Ref<Lattice::Settings> settings;
    Ref<ParticleDynamics::ParticleStorage> atoms;
    Slot<ParticleDynamics::IntegratorAPI> integrator;
    Slot<ParticleDynamics::SpatialIndexAPI> spatialGrid;
};

} // namespace ClassicMD
