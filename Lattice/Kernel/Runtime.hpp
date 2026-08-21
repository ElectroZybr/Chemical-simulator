#pragma once

#include <filesystem>
#include <string>
#include <iostream>

#include "Lattice/Kernel/ModelAPI.hpp"
#include "Lattice/Kernel/PluginManager.hpp"
#include "Lattice/Kernel/Components.hpp"
#include "Lattice/Kernel/Settings.hpp"
#include "Lattice/Tools/Logger.hpp"
#include <Lattice/Tools/SystemInfo.hpp>


namespace Lattice {
class Runtime {
public:
    Runtime() : root(&globalRegistry, nullptr) {
        Logger::action(moduleName, "System launching");
        Lattice::CliSystemInfo::printSystemInfo(std::cout);
        // регистрация интерфейсов ядра
        globalRegistry.registerAPI<ModelAPI>();
        globalRegistry.registerComponent<Settings>();
    }

    bool loadPlugins(std::filesystem::path path) {
        // загрузка внешних плагинов
        pluginManager.scanDirectory(path);
        pluginManager.checkCandidates();
        pluginManager.loadCandidates(globalRegistry);
        return true;
    }

    bool check(std::string_view modelId, std::string_view instanceName = "default") {
        Logger::action(moduleName, "Check requires {}:", modelId);
        Components branch(&globalRegistry, nullptr, Mode::Check, modelId);

        Component settings = branch.addComponent<Settings>();
        Component model = branch.addInterfaceSlot<ModelAPI>();
        Component created = branch.useInterface<ModelAPI>(modelId);
        branch.printRequirementTree();
        branch.printRequirements();
        for (const auto& r : branch.getUniqueRequirements()) {
            if (!globalRegistry.has(r.type)) {
                Logger::error(moduleName, "dependency check failed");
                return false;
            }
        }
        return true;
    }

    Component<ModelAPI> start(std::string_view modelId, std::string_view instanceName = "default") {
        Logger::Scope scope(moduleName, "Start modelAPI '{}' with name '{}'", modelId, instanceName);
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

    Registry& registry() noexcept { return globalRegistry; }

private:
    static constexpr std::string_view moduleName = "Runtime";
    Registry globalRegistry;
    PluginManager pluginManager;
    Components root;

    std::unordered_map<std::string, Component<ModelAPI>> models;
};
}
