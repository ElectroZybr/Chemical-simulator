#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Lattice/Kernel/ModelAPI.hpp"
#include "Lattice/Kernel/PluginManager.hpp"
#include "Lattice/Kernel/Component.hpp"

using id = size_t;

namespace Lattice {
class Runtime {
public:
    Runtime()
        : root(&globalRegistry)
    {}

    bool loadPlugins(std::filesystem::path path) {
        // регистрация интерфейсов ядра
        globalRegistry.registerAPI<ModelAPI>();
        // загрузка внешних плагинов
        pluginManager.scanDirectory(path);
        pluginManager.checkCandidates();
        pluginManager.loadCandidates(globalRegistry);
        return true;
    }

    Component<ModelAPI> start(std::string_view modelId, std::string_view instanceName = "default") {
        auto slot = root.add<ModelAPI>(instanceName);
        auto created = root.use<ModelAPI>(modelId, instanceName);
        if (!created)
            throw std::runtime_error("Failed to start model: " + std::string(modelId));

        models[std::string(instanceName)] = created;
        return created;
    }

    // Остановить и удалить
    void stop(std::string_view instanceName = "default") {
        models.erase(std::string(instanceName));
        // опционально: root_.remove<ModelAPI>(instanceName);
    }

    Component<ModelAPI> get(std::string_view instanceName = "default") {
        auto it = models.find(std::string(instanceName));
        if (it == models.end())
            return {};
        return it->second;
    }

    // Обновить все
    void updateAll() {
        for (auto& [name, model] : models) {
            if (model)
                model->update();
        }
    }

    // Обновить одну
    void update(std::string_view instanceName) {
        if (auto m = get(instanceName))
            m->update();
    }

    ModuleRegistry& registry() noexcept { return globalRegistry; }

private:
    static constexpr std::string_view moduleName = "Runtime";
    ModuleRegistry globalRegistry;
    PluginManager pluginManager;
    Components root;

    std::unordered_map<std::string, Component<ModelAPI>> models;
};
}
