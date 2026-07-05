#include "PluginInit.h"

#include "Integrators/Verlet.h"

extern "C" bool plugin_init(PluginHost& host, PluginInfo& info) {
    info.id = "classic_md_multithread";
    info.name = "Classic Molecular Dynamics MT";
    info.version = "0.1.0";

    return host.integrators.add(makeModuleMeta<IIntegrator, Verlet>());
}
