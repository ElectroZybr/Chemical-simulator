#include "Lattice/Kernel/PluginAPI.hpp"
#include "Plugins/ClassicMD/src/ClassicMD.hpp"
#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>

ClassicMD simulation;

extern "C" bool plugin_init(Kernel::PluginContext& context) {
    context.log("Hello from ClassicMD!");
    return true;
}

extern "C" void plugin_register(Kernel::PluginContext& ctx) {
    auto& integrator = ctx.getAPI<IntegratorAPI>();
    
    // На всякий случай проверка
    if (!integrator.step) {
        ctx.log("ERROR: IntegratorAPI::step is null");
        return;
    }

    simulation.setIntegrator(&integrator);
    simulation.step(0.01f);   // должно вызвать real_step
}

extern "C" void plugin_shutdown(Kernel::PluginContext& ctx) {
}