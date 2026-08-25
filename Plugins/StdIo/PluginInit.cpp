// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include "Lattice/Kernel/Registry.hpp"
#include "Lattice/Kernel/SubsystemAPI.hpp"

// Plugin dependences

// Sources
#include "IOSubsystem.hpp"
#include "LoaderAPI.hpp"
#include "ParserAPI.hpp"
#include "TomlParser.hpp"
// #include "JsonParser.hpp"
// #include "YamlParser.hpp"


extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerImpl<SubsystemAPI, IOSubsystem>();
    reg.registerAPI<LoaderAPI>();
    reg.registerAPI<ParserAPI>();
    reg.registerImpl<ParserAPI, TomlParser>();
    return true;
}

extern "C" void plugin_shutdown() {}