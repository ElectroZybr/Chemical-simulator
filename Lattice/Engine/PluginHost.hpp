#pragma once

#include "Lattice/Engine/physics/IForceField.h"
#include "Lattice/Engine/physics/IIntegrator.h"
#include "Lattice/Engine/physics/IThermostat.h"

struct PluginInfo {
    const char* id = "";
    const char* name = "";
    const char* version = "";
};

struct PluginHost {
    ModuleRegistry2<IForceField>& forceFields;
    ModuleRegistry2<IIntegrator>& integrators;
    ModuleRegistry2<IThermostat>& thermostats;
};

using PluginInitFn = bool (*)(PluginHost&, PluginInfo&);
