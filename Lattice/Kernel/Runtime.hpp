#pragma once

#include <filesystem>
#include <string>
#include <iostream>
#include <thread>

#include "Lattice/Kernel/ServiceAPI.hpp"
#include "Lattice/Kernel/SubsystemAPI.hpp"
#include "Lattice/Kernel/PluginManager.hpp"
#include "Lattice/Kernel/Components.hpp"
#include "Lattice/Kernel/Requirements.hpp"
#include <Lattice/Kernel/Exception.hpp>
#include "Lattice/Kernel/Settings.hpp"
#include "Lattice/Tools/LogStyle.hpp"
#include "Lattice/Tools/Logger.hpp"
#include <Lattice/Tools/SystemInfo.hpp>


namespace Lattice {
class Runtime {
public:
    Runtime() : root(&globalRegistry, nullptr) {
        Logger::action(tag, "System launching");
        Lattice::CliSystemInfo::printSystemInfo(std::cout);
        // регистрация интерфейсов ядра
        globalRegistry.registerAPI<ServiceAPI>();
        globalRegistry.registerAPI<SubsystemAPI>();
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

    SubsystemAPI& configure(std::string_view id, std::string_view instanceName = "default") {
        return root.add<SubsystemAPI>(id, instanceName);
    }

    bool check(std::string_view id) {
        return checkRequirements(id, globalRegistry);
    }

    ServiceAPI& start(std::string_view id, std::string_view instanceName = "default", ServiceLaunch launch = ServiceLaunch::Worker) {
        Logger::Scope scope(tag, "Start ServiceAPI '{}' with name '{}'", id, instanceName);
        ServiceAPI& service = root.add<ServiceAPI>(id, instanceName);

        if (launch == ServiceLaunch::Host) {
            if (!hostName.empty())
                throw Lattice::Exception("", "Runtime already has a host service");
            hostName = instanceName;
            Logger::info(tag, "Host service '{}'", instanceName);
        } else {
            service.start();
        }

        scope.finish("Configure '{}' done", instanceName);
        return service;
    }

    void run() {
        if (!hostName.empty()) {
            auto test = root.get<ServiceAPI>(hostName);

            Logger::info(
                tag,
                "Host lookup immediately after add: {}",
                test ? "FOUND" : "NOT FOUND"
            );
            auto* host = root.require<ServiceAPI>(hostName);
            host->enter();
            stopAll();
            return;
        }
        while (running)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

        stopAll();
    }

    void stop(std::string_view instanceName) {
        auto* service = root.get<ServiceAPI>(instanceName).get();

        if (!service)
            return;

        service->stop();
        root.remove<ServiceAPI>(instanceName);

        if (instanceName == hostName)
            hostName.clear();
    }

    ~Runtime() {
        stopAll();
    }

    Registry& registry() noexcept { return globalRegistry; }
    Components* roote() { return &root; }

    void reportException(const std::exception& error) const {
        auto* fatal = dynamic_cast<const Lattice::Exception*>(&error);
        if (fatal) {
            Logger::exception(fatal->tag(), "{}", error.what());
            Logger::message("{}Dump components tree (failed node is red):{}", Color::gray, Color::reset);
            root.dumpTree(fatal->tag());
        } else {
            Logger::exception(tag, "Unhandled exception: {}", error.what());
            Logger::message("{}Dump components tree{}", Color::gray, Color::reset);
            root.dumpTree();
        }
        Logger::message("\n{}{}Critical error. application terminated.{}", Color::red, Color::bold, Color::reset);
        Logger::message("{}{}Crash log: {}{}\n", Color::gray, Color::bold, std::string(Logger::logPath()), Color::reset);
    }

    void reportUnknownException() const {
        Logger::exception(tag, "Unhandled non-standard exception");
        Logger::treeLine("Dump components tree (failed node is red):");
        root.dumpTree();
    }

private:
    void stopAll() {
        running = false;
        root.stopServices();
    }

    static constexpr std::string_view tag = "Runtime";
    Registry globalRegistry;
    PluginManager pluginManager;
    Components root;

    bool running = true;
    std::string hostName;
};
}
