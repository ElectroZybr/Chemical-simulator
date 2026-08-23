#pragma once

#include <filesystem>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_map>

#include "Lattice/Kernel/ServiceAPI.hpp"
#include "Lattice/Kernel/PluginManager.hpp"
#include "Lattice/Kernel/Components.hpp"
#include "Lattice/Kernel/Requirements.hpp"
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
        root.add<Settings>();
    }

    bool loadPlugins(std::filesystem::path path) {
        // загрузка внешних плагинов
        pluginManager.scanDirectory(path);
        pluginManager.checkCandidates();
        pluginManager.loadCandidates(globalRegistry);
        return true;
    }

    bool check(std::string_view modelId) {
        return checkServiceRequirements(modelId, globalRegistry);
    }

    Slot<ServiceAPI> start(
        std::string_view modelId,
        std::string_view instanceName = "default",
        ServiceLaunch launch = ServiceLaunch::Worker)
    {
        Logger::Scope scope(moduleName, "Start ServiceAPI '{}' with name '{}'", modelId, instanceName);
        Slot<ServiceAPI> service = root.use<ServiceAPI>(modelId, instanceName);
        if (!service)
            throw std::runtime_error("Failed to start service: " + std::string(modelId));

        if (launch == ServiceLaunch::Host) {
            if (host)
                throw std::runtime_error("Runtime already has a host service");
            host = service;
            hostName = instanceName;
            Logger::info(moduleName, "Host service '{}'", instanceName);
        } else {
            service->start();
        }

        services[std::string(instanceName)] = service;
        scope.finish("Configure '{}' done", instanceName);
        return service;
    }

    void stop(std::string_view instanceName = "default") {
        const std::string key{instanceName};
        auto it = services.find(key);
        if (it == services.end()) {
            root.remove<ServiceAPI>(instanceName);
            return;
        }

        if (it->second)
            it->second->stop();

        const bool isHost = host && key == hostName;
        if (isHost && host->running())
            return;

        if (isHost)
            host = {};

        services.erase(it);
        root.remove<ServiceAPI>(instanceName);
    }

    void run() {
        if (host) {
            host->enter();
            stopAll();
            return;
        }

        while (running) {
            if (services.empty())
                break;

            bool allAlive = true;
            for (auto& [_, service] : services) {
                if (!service || !service->running()) {
                    allAlive = false;
                    break;
                }
            }
            if (!allAlive)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        stopAll();
    }

    ~Runtime() {
        stopAll();
    }

    Slot<ServiceAPI> get(std::string_view instanceName = "default") {
        auto it = services.find(std::string(instanceName));
        if (it == services.end())
            return {};
        return it->second;
    }

    Registry& registry() noexcept { return globalRegistry; }

private:
    void stopAll() {
        running = false;
        std::vector<std::string> names;
        names.reserve(services.size());
        for (auto& [name, _] : services)
            names.push_back(name);
        for (const auto& name : names)
            stop(name);
    }

    static constexpr std::string_view moduleName = "Runtime";
    Registry globalRegistry;
    PluginManager pluginManager;
    Components root;

    bool running = true;
    std::unordered_map<std::string, Slot<ServiceAPI>> services;
    Slot<ServiceAPI> host;
    std::string hostName;
};
}
