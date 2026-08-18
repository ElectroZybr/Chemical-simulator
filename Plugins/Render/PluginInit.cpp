// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>

// Plugin dependences

// Sources

namespace WGPURendering {

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {

    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {

}

} // ParticleDynamics