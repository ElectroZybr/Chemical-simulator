#include "Lattice/Kernel/PluginAPI.hpp"
#include "src/hello.cpp"

extern "C" bool plugin_init(Kernel::PluginContext& context) {

    context.log("Hello from RandomTestPlugin!");

    return true;
}