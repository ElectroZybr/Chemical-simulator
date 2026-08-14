#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Lattice/Kernel/Universe.hpp"
#include "Lattice/Kernel/UniverseModelAPI.hpp"
#include "Lattice/Kernel/PluginManager.hpp"

using id = size_t;

namespace Lattice {
class Runtime {
public:
    Runtime() = default;

    bool loadPlugins(std::filesystem::path path);

    /* === Управление мирами === */
    Universe& createUniverse();
    bool removeUniverse(id id);
    bool setActiveUniverse(id id);
    
    [[nodiscard]] id activeUniverse() { return activeUniverseId; }
    [[nodiscard]] size_t universeCount() const noexcept { return universes.size(); }
    
    [[nodiscard]] Universe& universeAt(id universeId);
    [[nodiscard]] const Universe& universeAt(id universeId) const;

    [[nodiscard]] Universe& universe();
    [[nodiscard]] const Universe& universe() const ;

private:
    static constexpr std::string_view moduleName = "Runtime";
    ModuleRegistry globalRegistry;
    PluginManager pluginManager;
    std::vector<Universe> universes;
    id activeUniverseId;
};
}
