#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

#include <Lattice/Kernel/DLLoader.hpp>
#include <Lattice/Kernel/Registry.hpp>
#include <Lattice/Kernel/Plugin.hpp>

namespace Lattice {

struct PluginManifest;

class PluginManager {
    static constexpr std::string_view tag = "PluginManager";
public:
    PluginManager(Registry& globalRegistry, DLLoader& dlLoader)
        : globalRegistry(globalRegistry), dlLoader(dlLoader) {}

    void scanDirectory(std::filesystem::path path);
    void checkCandidates();
    void loadCandidates();

    ~PluginManager();

    const Plugin* findCandidate(std::string_view id) const;
    const std::vector<Plugin*>& queue() const { return loadQueue; }

private:
    PluginManifest parseManifest(std::filesystem::path path);
    bool canLoad(const std::string& id);
    bool prepareLoad(const std::string& id);
    bool loadPlugin(Plugin* candidate);

    std::unordered_map<std::string, Plugin> candidates;
    std::vector<Plugin*> loadQueue;

    Registry& globalRegistry;
    DLLoader& dlLoader;
};
}