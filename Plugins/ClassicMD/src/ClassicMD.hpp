#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/Universe.hpp>
#include <Lattice/Kernel/UniverseModelAPI.hpp>

// Plugin dependences
#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>
#include <Plugins/Integrators/src/Verlet.hpp>

// Source

class ClassicMD final : public UniverseModelAPI {
public:
    static constexpr std::string_view id = "ClassicMD";

    void configure(Lattice::Universe& universe) override {
        integrator = universe.require<IntegratorAPI>();
        universe.use<IntegratorAPI>("Verlet");
        Log::ok(id, "Configure done");
    }

    void update() override {
        integrator->step(0.01f);
    }

private:
    Lattice::Slot<IntegratorAPI> integrator;
};