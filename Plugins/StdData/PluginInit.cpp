// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>

// Sources
#include "include/SoA.hpp"
// #include "include/CSR.hpp"


extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerComponent<StdData::SoA>();
    // reg.registerComponent<CSR>();
    return true;
}

extern "C" void plugin_shutdown() {}