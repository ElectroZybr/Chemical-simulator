#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Lattice/Kernel/ModelAPI.hpp"
#include "Lattice/Kernel/PluginManager.hpp"
#include "Lattice/Kernel/Components.hpp"
#include "Lattice/Kernel/Settings.hpp"
#include <Lattice/Tools/SystemInfo.hpp>

namespace Lattice {
class Runtime {
public:
    Runtime() : root(&globalRegistry) {
        Logger::action(moduleName, "System launching");
        Lattice::CliSystemInfo::printSystemInfo(std::cout);
        // регистрация интерфейсов ядра
        globalRegistry.registerAPI<ModelAPI>();
    }

    bool loadPlugins(std::filesystem::path path) {
        // загрузка внешних плагинов
        pluginManager.scanDirectory(path);
        pluginManager.checkCandidates();
        pluginManager.loadCandidates(globalRegistry);
        return true;
    }

    Component<ModelAPI> start(std::string_view modelId, std::string_view instanceName = "default") {
        LogScope scope(moduleName, "Start modelAPI '{}' with name '{}'", modelId, instanceName);
        Component branch = root.addBranch(instanceName);
        Component settings = branch->addComponent<Settings>();
        Component model = branch->addInterfaceSlot<ModelAPI>();
        Component created = branch->useInterface<ModelAPI>(modelId);
        if (!created)
            throw std::runtime_error("Failed to start model: " + std::string(modelId));

        models[std::string(instanceName)] = created;
        scope.finish("Configure '{}' done", instanceName);
        return created;
    }

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
        if (Component<ModelAPI> model = get(instanceName))
            model->update();
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
