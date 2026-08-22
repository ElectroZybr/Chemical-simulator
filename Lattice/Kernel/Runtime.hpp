#pragma once

#include <filesystem>
#include <string>
#include <iostream>
#include <thread>


#include "Lattice/Kernel/ServiceAPI.hpp"
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
        globalRegistry.registerAPI<ServiceAPI>();
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
        Components branch(&globalRegistry, nullptr, Mode::Check, modelId);

        branch.addComponent<Settings>();
        branch.addInterfaceSlot<ServiceAPI>();
        branch.useInterface<ServiceAPI>(modelId);
        for (const auto& r : branch.getUniqueRequirements()) {
            if (!globalRegistry.has(r.type)) {
                Logger::error(moduleName, "{} check failed", modelId);
                branch.printRequirements();
                return false;
            }
        }
        return true;
    }

    Component<ServiceAPI> start(std::string_view modelId, std::string_view instanceName = "default") {
        Logger::Scope scope(moduleName, "Start ServiceAPI '{}' with name '{}'", modelId, instanceName);
        Component branch = root.addBranch(instanceName);
        Component settings = branch->addComponent<Settings>();
        branch->addInterfaceSlot<ServiceAPI>();
        Component service = branch->useInterface<ServiceAPI>(modelId);
        if (!service)
            throw std::runtime_error("Failed to start service: " + std::string(modelId));

        service->start();
        services[std::string(instanceName)] = service;
        scope.finish("Configure '{}' done", instanceName);
        return service;
    }

    void stop(std::string_view instanceName = "default") {
        services.erase(std::string(instanceName));
        // опционально: root_.remove<ServiceAPI>(instanceName);
    }

    void run() {
        while (running) {
            Logger::info(moduleName, "looping");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    Component<ServiceAPI> get(std::string_view instanceName = "default") {
        auto it = services.find(std::string(instanceName));
        if (it == services.end())
            return {};
        return it->second;
    }

    Registry& registry() noexcept { return globalRegistry; }

private:
    static constexpr std::string_view moduleName = "Runtime";
    Registry globalRegistry;
    PluginManager pluginManager;
    Components root;

    bool running = true;
    std::unordered_map<std::string, Component<ServiceAPI>> services;
};
}
