#pragma once

#include <Lattice/Lattice.hpp>

namespace Lattice {


struct RuntimeFixture : public TestFixture {
    Registry registry;
    PluginManager pluginManager;
    Components root;

    RuntimeFixture() : root(&registry) {
        registry.registerAPI<ServiceAPI>();
        registry.registerAPI<SubsystemAPI>();
        registry.registerComponent<Settings>();
    }
};
}