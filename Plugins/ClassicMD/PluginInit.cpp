// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ModelAPI.hpp>

// Plugin dependences
#include <Plugins/ParticleDynamics/api/ParticleAPI.hpp>

// Source
#include "src/ClassicMD.hpp"

namespace ClassicMD {

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    reg.registerImpl<ModelAPI, ClassicMD>();
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    kernel.log("Hello from ClassicMD!");
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {
}

} // namespace ClassicMD