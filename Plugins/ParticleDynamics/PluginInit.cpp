// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>

// Plugin dependences
#include "src/ParticleAPI.hpp"
#include "src/DynamicSoALib.hpp"

namespace ParticleDynamics {

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    reg.registerAPI<IntegratorAPI>();
    reg.registerAPI<DynamicSoA>();
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    kernel.log("Hello from ParticleDynamics!");
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {

}

} // ParticleDynamics