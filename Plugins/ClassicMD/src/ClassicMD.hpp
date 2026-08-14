#pragma once

#include "Plugins/LatticeParticleAPI/ParticleAPI.hpp"
#include "Lattice/Kernel/PluginAPI.hpp"
#include "Lattice/Kernel/Universe.hpp"
#include "Lattice/Kernel/UniverseModelAPI.hpp"

class ClassicMD final : public UniverseModelAPI {
public:
    static constexpr std::string_view id = "ClassicMD";

    void configure(Lattice::Universe& universe) override {
        integrator = universe.require<IntegratorAPI>();
        Log::ok(id, "Configure done");
    }

    void update() override {
        Log::info(id, "Update world ClassocMD");
    }

private:
    // Lattice::KernelAPI& kernel;
    IntegratorAPI* integrator;
};