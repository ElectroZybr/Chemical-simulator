// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>

// Plugin dependences

// Sources
#include "include/ParticleAPI.hpp"
#include "src/SpatialGrid.hpp"

namespace ParticleDynamics {

extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerAPI<IntegratorAPI>();
    reg.registerAPI<ForceFieldAPI>();
    reg.registerAPI<SpatialIndexAPI>();

    reg.registerImpl<SpatialIndexAPI, SpatialGrid>();
    reg.registerComponent<ParticleStorage>();
    return true;
}

extern "C" void plugin_shutdown() {}
} // ParticleDynamics