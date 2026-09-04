#pragma once

#include <Lattice/Lattice.hpp>

namespace Lattice {


struct RuntimeFixture : public TestFixture {
    Registry registry;
    ObjectRegistry objectRegistry;
    PluginManager pluginManager;
    Node root;

    RuntimeFixture() : root(registry, objectRegistry) {
        registry.registerAPI<ServiceAPI>();
        registry.registerAPI<SubsystemAPI>();
        registry.registerComponent<Settings>();
    }
};
}