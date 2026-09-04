#pragma once

#include <Lattice/Lattice.hpp>

namespace Lattice {


struct RuntimeFixture : public TestFixture {
    DLLoader dlLoader;
    PluginManager pluginManager;
    Registry registry;
    ObjectRegistry objectRegistry;
    Node root;

    RuntimeFixture() : root(registry, objectRegistry) 
                     , pluginManager(registry, dlLoader) {
        registry.registerAPI<ServiceAPI>();
        registry.registerAPI<SubsystemAPI>();
        registry.registerComponent<Settings>();
    }
};
}