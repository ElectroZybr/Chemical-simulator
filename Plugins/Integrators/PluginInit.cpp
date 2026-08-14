#include <Lattice/Kernel/PluginAPI.hpp>
#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>
#include "Lattice/Log.hpp"

#include "Plugins/Integrators/src/Verlet.hpp"
#include "Plugins/Integrators/src/KDK.hpp"

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    reg.registerAPI<IntegratorAPI>();
    reg.registerImpl<IntegratorAPI, Verlet>();
    reg.registerImpl<IntegratorAPI, KDK>();
    // context.bind<IntegratorAPI, KDK>();
    // reg.bind<IntegratorAPI, Verlet>();
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    kernel.log("Hello from Integrators!");
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {
}