#include <Lattice/Kernel/PluginManager.hpp>
#include <Lattice/Kernel/Requirements.hpp>
#include <Lattice/Tools/Logger.hpp>
#include <Lattice/Kernel/Plugin.hpp>
#include <toml++/toml.h>

#include <cstddef>
#include <unordered_set>

namespace Lattice {
    void PluginManager::scanDirectory(std::filesystem::path path) {
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().filename() != "Plugin.toml")
                continue;
            
            try {
                std::filesystem::path pluginDir = entry.path().parent_path();
                Plugin candidate = {pluginDir, parseManifest(entry.path())};

                auto [it, inserted] = candidates.emplace(candidate.manifest.id, candidate);

                if (!inserted) {
                    Logger::error(tag, "Duplicate plugin id '{}': {}", candidate.manifest.id, entry.path().string());
                }

                Logger::info(tag, "Found plugin manifest: {} v{}", candidate.manifest.id, candidate.manifest.version.str());
            }
            catch (const std::exception& e) {
                Logger::error(tag, "Failed to parse {}: {}", entry.path().string(), e.what());
            }
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
        info.kernelApi = kernelApi;
        if (auto core = table["core"].as_table()) {
            if (auto apiNode = (*core)["api"]) {
                if (auto s = apiNode.value<std::string>())
                    info.kernelApi = VersionRange::parseVersion(*s);
                else if (auto n = apiNode.value<int64_t>())
                    info.kernelApi = Version{static_cast<uint8_t>(*n), 0, 0};
            }
        }

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

    void PluginManager::checkCandidates() {
        for (auto& [id, plugin] : candidates) {
            if (canLoad(id)) {
                prepareLoad(id);
            }
        }
    }

    bool PluginManager::canLoad(const std::string& id) {
        auto it = candidates.find(id);

        if (it == candidates.end()) {
            Logger::error("PluginManager", "Plugin '{}' not found", id);
            return false;
        }

        Plugin& plugin = it->second;

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

        if (plugin.manifest.kernelApi.major != kernelApi.major ||
            plugin.manifest.kernelApi > kernelApi) {
            plugin.status = LoadStatus::IncompatibleVersion;
            Logger::error(tag, "Plugin '{}' needs kernel API {}, host is {}",
                id, plugin.manifest.kernelApi.str(), kernelApi.str());
            return false;
        }

        for (const auto& dep : plugin.manifest.dependencies) {
            auto it = candidates.find(dep.id);
            if (it == candidates.end()) {
                plugin.status = LoadStatus::MissingDependency;
                Logger::error(tag, "Missing dependency '{}' for plugin '{}'", dep.id, id);
                return false;
            }

            Plugin& dependency = it->second;
            if (!dep.requirement.contains(dependency.manifest.version)) {
                plugin.status = LoadStatus::IncompatibleVersion;
                Logger::error(tag, "Plugin '{}' requires '{}' version {}, but found v{}", 
                    id, dep.id, dep.requirement.requirement, dependency.manifest.version.str());
                return false;
            }

            if (!canLoad(dep.id)) {
                plugin.status = it->second.status;
                Logger::error(tag, "Dependency '{}' cannot be loaded for plugin '{}'", dep.id, id);
                return false;
            }
        }

        plugin.status = LoadStatus::Valid;
        Logger::ok(tag, "Dependency check passed: {}", id);
        return true;
    }

    bool PluginManager::prepareLoad(const std::string& id) {
        auto it = candidates.find(id);

        if (it == candidates.end())
            return false;

        Plugin& plugin = it->second;

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

    void PluginManager::loadCandidates() {
        uint16_t loadedPlugins = 0; 
        for (Plugin* candidate : loadQueue) {
            Logger::Scope scope(tag, "Loading {}", candidate->path.string());
            if (loadPlugin(candidate)) {
                scope.finish("Loaded plugin {}", candidate->manifest.id);
                loadedPlugins++;
            } 
        }
        if (!loadedPlugins) {
            Logger::warning(tag, "No plugins could be loaded");
        } else {
            Logger::info(tag, "Loaded plugins: {}", loadedPlugins);
        }
    }

    bool PluginManager::loadPlugin(Plugin* candidate) {
        std::filesystem::path path;

        for (const auto& entry : std::filesystem::directory_iterator(candidate->path)) {
            if (entry.is_regular_file() && entry.path().extension() == DynamicLibrary::extension()) {
                path = entry.path();
                break;
            }
        }

        if (path.empty()) {
            Logger::error(tag, "No dynamic library found for plugin '{}': {}",
                candidate->manifest.id, candidate->path.string());
            candidate->status = LoadStatus::Failed;
            return false;
        }

        const size_t depsBefore = compileDepSink().size();

        auto* library = dlLoader.loadLibrary(path);
        if (!library) {
            candidate->status = LoadStatus::Failed;
            return false;
        }

        auto regFn = library->symbol<PluginRegisterFn>("plugin_register");
        if (!regFn) {
            Logger::error(tag, "plugin_register symbol not found in '{}'", path.string());
            candidate->status = LoadStatus::Failed;
            return false;
        }

        auto before = globalRegistry.listProvided();

        if (!regFn(globalRegistry)) {
            Logger::error(tag, "plugin_register failed for '{}'", candidate->manifest.id);
            candidate->status = LoadStatus::Failed;
            return false;
        }

        auto after = globalRegistry.listProvided();

        PluginCatalog catalog{.pluginId = candidate->manifest.id};

        const auto& sink = compileDepSink();
        catalog.deps.assign(sink.begin() + depsBefore, sink.end());

        std::unordered_set<std::string> beforeSet(before.begin(), before.end());
        for (const auto& name : after)
            if (!beforeSet.contains(name))
                catalog.provided.push_back(name);

        if (after.size() == before.size())
            Logger::warning(tag, "Plugin '{}' does not provide anything", candidate->manifest.id);

        recordPluginCatalog(std::move(catalog));

        candidate->status = LoadStatus::Loaded;
        return true;
    }

    PluginManager::~PluginManager() {
        for (Plugin* plugin : loadQueue) {
            if (plugin->status != LoadStatus::Loaded)
                continue;

            if (plugin->shutdown)
                plugin->shutdown();
        }
    }

    const Plugin* PluginManager::findCandidate(std::string_view id) const {
        auto it = candidates.find(std::string(id));

        if (it == candidates.end())
            return nullptr;

        return &it->second;
    }

}