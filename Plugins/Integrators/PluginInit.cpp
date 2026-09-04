#include "Plugins/Integrators/src/Verlet.hpp"
#include "Plugins/Integrators/src/KDK.hpp"

#include <Lattice/Kernel/Plugin.hpp>
#include <Lattice/Tools/Logger.hpp>

#include <ParticleDynamics/include/ParticleAPI.hpp>

namespace Integrators {

extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerImpl<ParticleDynamics::IntegratorAPI, Verlet>();
    reg.registerImpl<ParticleDynamics::IntegratorAPI, KDK>();
    return true;
}

extern "C" void plugin_shutdown() {}
}