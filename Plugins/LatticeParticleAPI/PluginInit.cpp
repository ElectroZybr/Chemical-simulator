#include <Lattice/Kernel/PluginAPI.hpp>

#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    reg.registerAPI<IntegratorAPI>();
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    kernel.log("Hello from LatticeParticleAPI!");
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {

}