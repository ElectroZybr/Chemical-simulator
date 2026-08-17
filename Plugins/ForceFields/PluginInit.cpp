#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Log.hpp>

namespace ForceFields {

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    kernel.log("Hello from ForceFields!");
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {
}

}