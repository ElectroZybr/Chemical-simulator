#include "Lattice/Kernel/PluginManager.hpp"
#include "Lattice/Log.hpp"

namespace Kernel {
    void PluginManager::scanDirectory(std::filesystem::path path) {
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().filename() != "Plugin.toml")
                continue;
            
            try {
                std::filesystem::path pluginDir = entry.path().parent_path();
                PluginCandidate candidate = {pluginDir, parseManifest(entry.path())};

                auto [it, inserted] = candidates.emplace(candidate.manifest.id, candidate);

                if (!inserted) {
                    Log::error(moduleName, "Duplicate plugin id '{}': {}", candidate.manifest.id, entry.path().string());
                }

                Log::info(moduleName, "Found plugin: {} v{}", candidate.manifest.id, candidate.manifest.version.str());
            }
            catch (const std::exception& e) {
                Log::error(moduleName, "Failed to parse {}: {}", entry.path().string(), e.what());
            }
        }
    }

    void PluginManager::checkCandidates() {
        for (auto& [id, plugin] : candidates) {
            if (canLoad(id)) {
                prepareLoad(id);
            }
        }
    }

    void PluginManager::loadCandidates() {
        uint16_t loadedPlugins = 0; 
        for (PluginCandidate* plugin : loadQueue) {
            if (loadPlugin(plugin)) {
                Log::ok(moduleName, "Loaded plugin {}", plugin->manifest.id);
                loadedPlugins++;
            }
        }
        if (!loadedPlugins) {
            Log::warning(moduleName, "No plugins could be loaded");
        } else {
            Log::info(moduleName, "Loaded plugins: {}", loadedPlugins);
        }
    }

    PluginManifest PluginManager::parseManifest(std::filesystem::path path) {
        PluginManifest info;
        auto table = toml::parse_file(path.string());

        info.id = table["id"].value_or("");
        info.name = table["name"].value_or("");
        if (auto version = table["version"].value<std::string>()) {
            info.version = VersionRange::parseVersion(*version);
        }
        info.kernelApiVersion = table["api"].value_or(1);

        if (auto deps = table["dependencies"].as_array()) {
            for (auto&& node : *deps) {
                auto* dep = node.as_table();
                if (!dep)
                    continue;

                PluginDependency dependency;

                dependency.id = dep->at("id").value_or("");

                if (auto version = dep->at("version").value<std::string>()) {
                    dependency.requirement = VersionRange::parse(*version);
                }

                info.dependencies.push_back(std::move(dependency));
            }
        }
        return info;
    }

    bool PluginManager::canLoad(const std::string& id) {
        auto it = candidates.find(id);

        if (it == candidates.end()) {
            Log::error("PluginManager", "Plugin '{}' not found", id);
            return false;
        }

        PluginCandidate& plugin = it->second;

        if (plugin.status == LoadStatus::Valid ||
            plugin.status == LoadStatus::Loaded ||
            plugin.status == LoadStatus::Queued) {
            return true;
        }

        if (plugin.status == LoadStatus::DependencyCycle) {
            Log::error("PluginManager", "Circular dependency detected at '{}'", id);
            return false;
        }

        if (plugin.status != LoadStatus::NonChecked)
            return false;

        plugin.status = LoadStatus::DependencyCycle;

        for (const auto& dep : plugin.manifest.dependencies) {
            auto it = candidates.find(dep.id);
            if (it == candidates.end()) {
                plugin.status = LoadStatus::MissingDependency;
                Log::error(moduleName, "Missing dependency '{}' for plugin '{}'", dep.id, id);
                return false;
            }

            PluginCandidate& dependency = it->second;
            if (!dep.requirement.contains(dependency.manifest.version)) {
                plugin.status = LoadStatus::IncompatibleVersion;
                Log::error(moduleName, "Plugin '{}' requires '{}' version {}, but found v{}", 
                    id, dep.id, dep.requirement.requirement, dependency.manifest.version.str());
                return false;
            }

            if (!canLoad(dep.id)) {
                plugin.status = it->second.status;
                Log::error(moduleName, "Dependency '{}' cannot be loaded for plugin '{}'", dep.id, id);
                return false;
            }
        }

        plugin.status = LoadStatus::Valid;
        Log::ok(moduleName, "Dependency check passed: {}", id);
        return true;
    }

    bool PluginManager::prepareLoad(const std::string& id) {
        auto it = candidates.find(id);

        if (it == candidates.end())
            return false;

        PluginCandidate& plugin = it->second;

        if (plugin.status == LoadStatus::Queued ||
            plugin.status == LoadStatus::Loaded) {
            return true;
        }

        for (const auto& dep : plugin.manifest.dependencies) {
            if (!prepareLoad(dep.id))
                return false;
        }

        plugin.status = LoadStatus::Queued;
        loadQueue.push_back(&plugin);

        return true;
    }

    bool PluginManager::loadPlugin(const PluginCandidate* pluginCandidate) {
        std::filesystem::path pluginPath;
        for (const auto& entry : std::filesystem::directory_iterator(pluginCandidate->path)) {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().extension() == DynamicLibrary::extension()) {
                pluginPath = entry.path();
                break;
            }
        } 
        
        if (pluginPath.empty()) {
            Log::error(moduleName, "No regular file to: {}", pluginPath.string());
            return false;
        }

        Log::action(moduleName, "Loading {}", pluginPath.string());

        DynamicLibrary library;
        if (!library.open(pluginPath)) {
            Log::error(moduleName, "Failed to open '{}': {}", pluginPath.string(), library.lastError());
            return false;
        }

        PluginInitFn init = library.symbol<PluginInitFn>("plugin_init");
        if (init == nullptr) {
            Log::error(moduleName, "Missing symbol 'plugin_init' in '{}': {}", pluginPath.string(), library.lastError());
            return false;
        }

        std::vector<std::string> allowed;
        for (const PluginDependency& dep : pluginCandidate->manifest.dependencies) {
            allowed.push_back(dep.id);
        }
        PluginContext context(globalRegistry, allowed);
        if (!init(&context)) {
            Log::error(moduleName, "plugin_init failed for '{}'", pluginPath.string());
            return false;
        }
        
        LoadedPlugin plugin(std::move(library), std::move(context));
        plugin.init = init;
        PluginRegisterFn reg = plugin.library.symbol<PluginRegisterFn>("plugin_register");
        if (reg) {
            reg(&plugin.context);
        }
        plugin.shutdown = plugin.library.symbol<PluginShutdownFn>("plugin_shutdown");
        plugins.push_back(std::move(plugin));

        return true;
    }

    PluginManager::~PluginManager() {
        for (LoadedPlugin& plugin : plugins) {
            if (plugin.shutdown)
                plugin.shutdown(&plugin.context);
        }
        plugins.clear();
    }

}