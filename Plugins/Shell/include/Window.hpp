#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
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
    explicit Window(Lattice::Components& branch)
        : settings(*branch.require<Lattice::Settings>())
        , window(branch.use<WindowAPI, glfwWindow>())
        , render(branch.add<Render>())
    {
        Logger::info("Window", "window created");
        settings.on("actions", "print", [&]() { print(); });
        // actionMap.set("actions", "print");

    }

    void configure(Lattice::Components& branch) {
        actionMap = branch.require<ActionMap>();
        window->show();
        window->setTitle("LatticeLab");

        actionMap->set("verlet.dt");
        actionMap->set("actions.print");
        actionMap->set("io.load");
        actionMap->bindAdd("verlet.dt", "MouseLeft", +0.001);
        actionMap->bind("actions.print", "Ctrl+S");
        actionMap->bind("io.load", "Ctrl+O");
    }

    void run() override {
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
    Lattice::Settings& settings;
    Lattice::Slot<WindowAPI> window;
    Render* render = nullptr;
    ActionMap* actionMap;
    uint32_t fps;
    void print() {
        Logger::action("printer", "test");
    }
};