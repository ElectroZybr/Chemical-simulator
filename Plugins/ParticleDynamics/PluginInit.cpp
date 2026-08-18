// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>

// Plugin dependences

// Sources
#include "api/ParticleAPI.hpp"
#include "src/SpatialGrid.hpp"

namespace ParticleDynamics {

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    reg.registerAPI<IntegratorAPI>();
    reg.registerAPI<ForceFieldAPI>();
    reg.registerAPI<SpatialIndexAPI>();

    reg.registerImpl<SpatialIndexAPI, SpatialGrid>();
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {

}

} // ParticleDynamics