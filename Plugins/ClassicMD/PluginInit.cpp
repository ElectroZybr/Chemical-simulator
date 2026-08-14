// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/UniverseModelAPI.hpp>

// Plugin dependences
#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>

// Source
#include "src/ClassicMD.hpp"

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    reg.registerImpl<UniverseModelAPI, ClassicMD>();
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    kernel.log("Hello from ClassicMD!");
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {
}