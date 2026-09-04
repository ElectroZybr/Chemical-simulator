// Kernel dependences
#include <Lattice/Kernel/Plugin.hpp>
#include "Lattice/Kernel/Registry.hpp"


extern "C" bool plugin_register(Lattice::Registry& reg) {
    return true;
}

extern "C" void plugin_shutdown() {}