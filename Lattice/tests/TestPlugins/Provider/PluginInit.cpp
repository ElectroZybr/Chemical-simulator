// Kernel dependences
#include <Lattice/Kernel/Plugin.hpp>
#include "Lattice/Kernel/Registry.hpp"


struct TestAPI {
    static constexpr std::string_view apiName = "TestAPI";
};

extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerAPI<TestAPI>();
    return true;
}

extern "C" void plugin_shutdown() {}