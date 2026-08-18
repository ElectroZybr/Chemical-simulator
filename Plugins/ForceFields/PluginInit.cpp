#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Tools/Logger.hpp>

namespace ForceFields {

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {
}

}