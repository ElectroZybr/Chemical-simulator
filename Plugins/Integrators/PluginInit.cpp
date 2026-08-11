#include <Lattice/Kernel/PluginAPI.hpp>
#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>
#include "Lattice/Log.hpp"

#include "Plugins/Integrators/src/Verlet.hpp"

void verletStep(void* instance, float dt) {
    static_cast<Verlet*>(instance)->step(dt);
}

static IntegratorAPI api {
    .instance = nullptr,
    .step = &verletStep
};

extern "C" bool plugin_init(Kernel::PluginContext& context) {
    context.log("Hello from Integrators!");
    return true;
}

extern "C" void plugin_register(Kernel::PluginContext& context) {
    context.registerAPI<IntegratorAPI>(&api);
}

extern "C" void plugin_shutdown(Kernel::PluginContext& context) {
}