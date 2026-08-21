#include <Lattice/Kernel/PluginManager.hpp>
#include <Lattice/Tools/Logger.hpp>
#include <toml++/toml.h>

namespace Lattice {
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
                    Logger::error(moduleName, "Duplicate plugin id '{}': {}", candidate.manifest.id, entry.path().string());
                }

                Logger::info(moduleName, "Found plugin manifest: {} v{}", candidate.manifest.id, candidate.manifest.version.str());
            }
            catch (const std::exception& e) {
                Logger::error(moduleName, "Failed to parse {}: {}", entry.path().string(), e.what());
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

    void PluginManager::loadCandidates(Registry& globalRegistry) {
        uint16_t loadedPlugins = 0; 
        for (PluginCandidate* plugin : loadQueue) {
            if (loadPlugin(plugin, globalRegistry)) {
                Logger::ok(moduleName, "Loaded plugin {}", plugin->manifest.id);
                loadedPlugins++;
            }
        }
        if (!loadedPlugins) {
            Logger::warning(moduleName, "No plugins could be loaded");
        } else {
            Logger::info(moduleName, "Loaded plugins: {}", loadedPlugins);
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
            Logger::error("PluginManager", "Plugin '{}' not found", id);
            return false;
        }

        PluginCandidate& plugin = it->second;

        if (plugin.status == LoadStatus::Valid ||
            plugin.status == LoadStatus::Loaded ||
            plugin.status == LoadStatus::Queued) {
            return true;
        }

        if (plugin.status == LoadStatus::DependencyCycle) {
            Logger::error("PluginManager", "Circular dependency detected at '{}'", id);
            return false;
        }

        if (plugin.status != LoadStatus::NonChecked)
            return false;

        plugin.status = LoadStatus::DependencyCycle;

        for (const auto& dep : plugin.manifest.dependencies) {
            auto it = candidates.find(dep.id);
            if (it == candidates.end()) {
                plugin.status = LoadStatus::MissingDependency;
                Logger::error(moduleName, "Missing dependency '{}' for plugin '{}'", dep.id, id);
                return false;
            }

            PluginCandidate& dependency = it->second;
            if (!dep.requirement.contains(dependency.manifest.version)) {
                plugin.status = LoadStatus::IncompatibleVersion;
                Logger::error(moduleName, "Plugin '{}' requires '{}' version {}, but found v{}", 
                    id, dep.id, dep.requirement.requirement, dependency.manifest.version.str());
                return false;
            }

            if (!canLoad(dep.id)) {
                plugin.status = it->second.status;
                Logger::error(moduleName, "Dependency '{}' cannot be loaded for plugin '{}'", dep.id, id);
                return false;
            }
        }

        plugin.status = LoadStatus::Valid;
        Logger::ok(moduleName, "Dependency check passed: {}", id);
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

    bool PluginManager::loadPlugin(PluginCandidate* pluginCandidate, Registry& globalRegistry) {
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
            Logger::error(
                moduleName,
                "No dynamic library found for plugin '{}': {}",
                pluginCandidate->manifest.id,
                pluginCandidate->path.string()
            );

            pluginCandidate->status = LoadStatus::Failed;
            return false;
        }

        Logger::action(moduleName, "Loading {}", pluginPath.string());

        DynamicLibrary library;
        if (!library.open(pluginPath)) {
            Logger::error(moduleName, "Failed to open '{}': {}", pluginPath.string(), library.lastError());
            pluginCandidate->status = LoadStatus::Failed;
            return false;
        }

        PluginRegisterFn regFn = library.symbol<PluginRegisterFn>("plugin_register");
        if (!regFn) {
            Logger::error(moduleName, "plugin_register symbol not found in '{}'", pluginPath.string());
            pluginCandidate->status = LoadStatus::Failed;
            return false;
        }

        // Сверим, что registry меняется: запомним текущее состояние, вызовем регистрацию и посмотрим на разницу
        auto before = globalRegistry.listProvided();
        if (!regFn(&globalRegistry)) {
            Logger::error(moduleName, "plugin_register failed for '{}'", pluginPath.string());
            pluginCandidate->status = LoadStatus::Failed;
            return false;
        }

        auto after = globalRegistry.listProvided();
        // Если ничего не добавилось — предупреждение
        if (after.size() == before.size()) {
            Logger::warning(moduleName, "Plugin '{}' does not provide anything", pluginCandidate->manifest.id);
        }

        PluginShutdownFn shutdown = library.symbol<PluginShutdownFn>("plugin_shutdown");
        plugins.push_back(LoadedPlugin{std::move(library), pluginCandidate->manifest, shutdown});
        pluginCandidate->status = LoadStatus::Loaded;

        return true;
    }

    PluginManager::~PluginManager() {
        for (LoadedPlugin& plugin : plugins) {
            if (plugin.shutdown)
                plugin.shutdown();
        }
        plugins.clear();
    }

}