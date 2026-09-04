#include <Lattice/Kernel/Plugin.hpp>
#include <Lattice/Tools/Logger.hpp>

namespace ForceFields {

extern "C" bool plugin_register(Lattice::Registry& reg) {
    return true;
}

extern "C" void plugin_shutdown() {}
}