#include "Lattice/Kernel/PluginAPI.hpp"
#include "Lattice/Log.hpp"
#include "Plugins/Physics/src/Physics.hpp"

extern "C" bool plugin_init(Kernel::PluginContext& ctx) {

    ctx.log("Hello from RandomTestPlugin!");

    return true;
}

extern "C" void plugin_register(Kernel::PluginContext& ctx) {
    Physics& physics = ctx.getAPI<Physics>();
    physics.simulate(0.01f);
}

extern "C" void plugin_shutdown(Kernel::PluginContext& ctx) {

}