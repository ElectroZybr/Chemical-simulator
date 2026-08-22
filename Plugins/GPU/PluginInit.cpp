// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>

// Sources
#include "Lattice/Kernel/Registry.hpp"
#include "include/WGPU.hpp"

namespace GPU {

extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerComponent<WGPU>();
    return true;
}

extern "C" void plugin_shutdown() {}
} // GPU