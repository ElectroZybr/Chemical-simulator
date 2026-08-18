// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>

// Sources
#include "src/backend/GPUAPI.hpp"
#include "src/backend/WGPU.hpp"

namespace GPU {

extern "C" bool plugin_register(Lattice::PluginRegister& reg) {
    reg.registerAPI<GPUAPI>();
    reg.registerImpl<GPUAPI, WGPU>();
    return true;
}

extern "C" bool plugin_init(Lattice::KernelAPI& kernel) {
    return true;
}

extern "C" void plugin_shutdown(Lattice::KernelAPI& kernel) {

}

} // GPU