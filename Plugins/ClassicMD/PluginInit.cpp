// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>

// Plugin dependences
#include <Plugins/ParticleDynamics/api/ParticleAPI.hpp>

// Source
#include "src/ClassicMD.hpp"

namespace ClassicMD {

extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerImpl<ServiceAPI, ClassicMD>();
    return true;
}

extern "C" void plugin_shutdown() {}
} // namespace ClassicMD