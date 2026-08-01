#pragma once

#include <filesystem>
#include <utility>
#include <vector>
#include <string_view>

#include "Lattice/Engine/DynamicLibrary.hpp"
#include "Lattice/Engine/PluginHost.hpp"
#include "Lattice/Log.hpp"
#include "Lattice/Engine/physics/IForceField.h"
#include "Lattice/Engine/physics/IIntegrator.h"
#include "Lattice/Engine/physics/IThermostat.h"

class PluginLoader {
public:
    PluginLoader()
        : host_{
              ForceField::registry(),
              Integrator::registry(),
              Thermostat::registry(),
          } {}

    int load(const std::filesystem::path& pluginsDir) {
        int loadedCount = 0;

        if (!std::filesystem::exists(pluginsDir) || !std::filesystem::is_directory(pluginsDir)) {
            Log::warning("PluginLoader", "Plugins directory is missing or not a directory: {}", pluginsDir.string());
            return 0;
        }

        Log::action("PluginLoader", "Scanning {}...", pluginsDir.string());

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(pluginsDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const std::filesystem::path libraryPath = entry.path();
            if (libraryPath.extension() != DynamicLibrary::extension()) {
                continue;
            }

            Log::action("PluginLoader", "Loading {}", libraryPath.string());

            DynamicLibrary library;
            if (!library.open(libraryPath)) {
                Log::error("PluginLoader", "Failed to open '{}': {}", libraryPath.string(), library.lastError());
                continue;
            }

            PluginInitFn init = library.symbol<PluginInitFn>("plugin_init");
            if (init == nullptr) {
                Log::error("PluginLoader", "Missing symbol 'plugin_init' in '{}': {}", libraryPath.string(), library.lastError());
                continue;
            }

            if (!init(host_, library.info)) {
                Log::error("PluginLoader", "plugin_init failed for '{}'", libraryPath.string());
                continue;
            }

            Log::info(
                "PluginLoader",
                "Registry state after {}: forceFields={} integrators={} thermostats={}",
                library.info.id != nullptr && library.info.id[0] != '\0' ? library.info.id : libraryPath.filename().string(),
                host_.forceFields.items().size(),
                host_.integrators.items().size(),
                host_.thermostats.items().size());

            if (library.info.id != nullptr && std::string_view(library.info.id) == "classic_md" && host_.forceFields.find("classic_md") == nullptr) {
                Log::warning("PluginLoader", "Plugin classic_md loaded, but force field 'classic_md' is not registered");
            }

            Log::info(
                "PluginLoader",
                "Loaded \"{}\" id={} version={}",
                library.info.name != nullptr && library.info.name[0] != '\0' ? library.info.name : "<unnamed>",
                library.info.id != nullptr && library.info.id[0] != '\0' ? library.info.id : "<none>",
                library.info.version != nullptr && library.info.version[0] != '\0' ? library.info.version : "<none>");

            loadedPlugins_.push_back(std::move(library));
            ++loadedCount;
        }

        if (loadedCount == 0) {
            Log::warning("PluginLoader", "No plugins were loaded from {}", pluginsDir.string());
        } else {
            Log::ok("PluginLoader", "Plugins loaded: {}", loadedCount);
        }
        return loadedCount;
    }

    const std::vector<DynamicLibrary>& loadedPlugins() const noexcept { return loadedPlugins_; }

private:
    PluginHost host_;
    std::vector<DynamicLibrary> loadedPlugins_;
};
