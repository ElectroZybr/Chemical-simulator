// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ModelAPI.hpp>

// Plugin dependences
#include <Plugins/ParticleDynamics/api/ParticleAPI.hpp>

// Source
#include "src/ClassicMD.hpp"

namespace ClassicMD {

extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerImpl<ModelAPI, ClassicMD>();
    return true;
}

extern "C" void plugin_shutdown() {}
} // namespace ClassicMD