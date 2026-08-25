// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include "Lattice/Kernel/Registry.hpp"
#include "Lattice/Kernel/SubsystemAPI.hpp"

// Plugin dependences

// Sources
#include "InputAPI.hpp"
#include "ActionMap.hpp"
#include "CommandSlots.hpp"


extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerAPI<InputAPI>();
    reg.registerImpl<SubsystemAPI, ActionMap>();
    reg.registerComponent<CommandSlots>();
    return true;
}

extern "C" void plugin_shutdown() {}