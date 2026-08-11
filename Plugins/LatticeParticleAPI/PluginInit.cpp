#include <Lattice/Kernel/PluginAPI.hpp>
#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>

extern "C" bool plugin_init(Kernel::PluginContext& context) {

    context.log("Hello from LatticeParticleAPI!");

    return true;
}

extern "C" void plugin_register(Kernel::PluginContext& context) {
}

extern "C" void plugin_shutdown(Kernel::PluginContext& context) {

}