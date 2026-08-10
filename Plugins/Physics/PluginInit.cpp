#include "Lattice/Kernel/PluginAPI.hpp"
#include "src/Physics.hpp"

extern "C" bool plugin_init(Kernel::PluginContext& context) {

    context.log("Hello from Physics!");

    return true;
}

extern "C" void plugin_register(Kernel::PluginContext& ctx) {
    ctx.registerAPI<Physics>(&physics);
}

extern "C" void plugin_shutdown(Kernel::PluginContext& ctx) {

}