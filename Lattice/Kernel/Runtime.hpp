#pragma once

#include <filesystem>
#include <string>
#include <iostream>
#include <thread>

#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/SubsystemAPI.hpp>
#include <Lattice/Kernel/PluginManager.hpp>
#include <Lattice/Kernel/StartupConfig.hpp>
#include <Lattice/Kernel/Requirements.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Exception.hpp>
#include <Lattice/Kernel/Settings.hpp>
#include <Lattice/Tools/SystemInfo.hpp>
#include "Lattice/Tools/LogStyle.hpp"
#include "Lattice/Tools/Logger.hpp"
#include "Lattice/Tools/Tests.hpp"


namespace Lattice {
class Runtime {
    static constexpr std::string_view tag = "Runtime";
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

    void buildBranch(const StartupEntry& entry, std::string_view name = "default") {
        Logger::Scope scope(tag, "Start component '{}' with name '{}'", entry.name, name);
        if (globalRegistry.hasImpl<ServiceAPI>(entry.name)) {
            root.add<ServiceAPI>(entry.name, name);

            if (entry.host) {
                if (!hostName.empty())
                    throw Lattice::Exception(tag, "Runtime already has a host service");

                hostName = entry.name;
                Logger::info(tag, "Host service '{}'", entry.name);
            }

            scope.finish("Start '{}' done", entry.name);
            return;
        }

        if (globalRegistry.hasImpl<SubsystemAPI>(entry.name)) {
            root.add<SubsystemAPI>(entry.name, name);
            scope.finish("Start '{}' done", entry.name);
            return;
        }

        throw Lattice::Exception(tag, "unknown component '{}'", entry.name);
    }

    void startServices(const StartupConfig& config) {
        for (const auto& entry : config.entries()) {
            if (!entry.enabled || entry.host)
                continue;

            if (!globalRegistry.hasImpl<ServiceAPI>(entry.name))
                continue;

            auto service = root.require<ServiceAPI>(entry.name);
            service->start();

            Logger::info(tag, "Started service '{}'", entry.name);
        }
    }

    void run(int argc, char** argv) {
        try {
            Logger::ConsoleMode consoleMode = Logger::ConsoleMode::Trace;
            std::filesystem::path configPath = "lattice.toml";
            bool testMode = false;
            for (int i = 1; i < argc; ++i) {
                const std::string_view arg = argv[i];
                if (arg == "--trace") {
                    consoleMode = Logger::ConsoleMode::Trace;
                } else if (arg == "--verbose" || arg == "-v") {
                    if (consoleMode != Logger::ConsoleMode::Trace) {
                        consoleMode = Logger::ConsoleMode::Verbose;
                    }
                } else if (arg == "--config" || arg == "-c") {
                    if (++i >= argc)
                        throw Lattice::Exception(tag, "missing path for {}", arg);
                    configPath = argv[i];
                } else if (arg == "--tests" || arg == "-t") {
                    testMode = true;
                }
            }

            Logger::setConsoleMode(consoleMode);
            StartupConfig config(configPath);


            loadPlugins("Plugins");
            if (testMode) {
                TestRegistry::instance().runAll();
                return;
            }

            for (const auto& entry : config.entries())
                buildBranch(entry);
            root.configureAll();
            startServices(config);

            if (!hostName.empty()) {
                auto host = root.require<ServiceAPI>(hostName);
                host->enter();
            } else {
                while (running)
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            stopAll();
        } catch (const std::exception& error) {
            reportException(error);
        } catch (...) {
            reportUnknownException();
        }
    }

    void stop(std::string_view instanceName) {
        auto service = root.find<ServiceAPI>(instanceName).get();

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

    void reportException(const std::exception& error) const {
        auto* fatal = dynamic_cast<const Lattice::Exception*>(&error);
        Logger::message("\n{}~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{}", Color::red, Color::reset);
        if (fatal) {
            Logger::exception(fatal->tag(), "{}", error.what());
            Logger::message("{}Dump components tree (failed node is red):{}", Color::gray, Color::reset);
            root.dumpTree(fatal->tag());
        } else {
            Logger::exception(tag, "Unhandled exception: {}", error.what());
            Logger::message("{}Dump components tree{}", Color::gray, Color::reset);
            root.dumpTree();
        }
        Logger::message("\n{}{}Critical error. Application terminated.{}", Color::red, Color::bold, Color::reset);
        Logger::message("{}{}Crash log: {}{}", Color::gray, Color::bold, std::string(Logger::logPath()), Color::reset);
        Logger::message("{}~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{}\n", Color::red, Color::reset);
    }

    void reportUnknownException() const {
        Logger::exception(tag, "Unhandled non-standard exception");
        Logger::message("Dump components tree (failed node is red):");
        root.dumpTree();
    }

private:
    void stopAll() {
        running = false;
        root.stopServices();
    }

    Registry globalRegistry;
    PluginManager pluginManager;
    Components root;

    bool running = true;
    std::string hostName;
};
}
