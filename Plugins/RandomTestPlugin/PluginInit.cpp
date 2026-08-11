#include "Lattice/Kernel/PluginAPI.hpp"
#include "Lattice/Log.hpp"

extern "C" bool plugin_init(Kernel::PluginContext& ctx) {

    ctx.log("Hello from RandomTestPlugin!");

    return true;
}

extern "C" void plugin_register(Kernel::PluginContext& ctx) {
}

extern "C" void plugin_shutdown(Kernel::PluginContext& ctx) {

}