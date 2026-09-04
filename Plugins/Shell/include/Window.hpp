#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Node.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences
// #include "Document.hpp"
// #include "TomlParser.hpp"
#include "ActionMap.hpp"

// Source
#include "Render.hpp"
#include "WindowAPI.hpp"
#include "glfwWindow/glfwWindow.hpp"


class Window final : public ServiceAPI {
public:
    explicit Window(Lattice::Node& branch) {
        branch.use<WindowAPI, glfwWindow>();
        branch.add<Render>();
        Logger::info("Window", "window created");
    }

    void configure(Lattice::Node& branch) {
        settings = branch.require<Lattice::Settings>();
        actionMap = branch.require<ActionMap>();
        render = branch.require<Render>();
        window = branch.find<WindowAPI>();

        settings->on("actions", "print", [&]() { print(); });

        window->show();
        window->setTitle("LatticeLab");


    }

    void run() override {
        actionMap->set("verlet.dt");
        actionMap->set("actions.print");
        actionMap->set("io.load");
        actionMap->bindAdd("verlet.dt", "MouseLeft", +0.001);
        actionMap->bind("actions.print", "Ctrl+S");
        actionMap->bind("io.load", "Ctrl+O");
        render->setup();
        while (!stopRequested()) {
            window->pollEvents();
            actionMap->tick();
            if (window->shouldClose()) {
                requestStop();
                break;
            }
            render->frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    ~Window() {
        stop();
        Logger::info("Window", "destroying object");
    }

private:
    Ref<Lattice::Settings> settings;
    Ref<ActionMap> actionMap;
    Ref<Render> render;
    Slot<WindowAPI> window;

    uint32_t fps;
    void print() {
        Logger::action("printer", "test");
    }
};