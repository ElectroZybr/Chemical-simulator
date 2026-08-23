#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

#include "ActionMap.hpp"
#include "Render.hpp"

#include "WindowAPI.hpp"
#include "Render/include/Render.hpp"
#include "Shell/src/glfwWindow/glfwWindow.hpp"

class Window final : public ServiceAPI {
public:
    explicit Window(Lattice::Components& branch)
        : settings(*branch.require<Lattice::Settings>())
        , actionMap(settings)
    {
        window = branch.use<WindowAPI, glfwWindow>();
        render = branch.add<Render>();
        Logger::info("Window", "window created");
        settings.on("print", "dfd", [&]() { print(); });

        actionMap.bindAction("print.dfd", "P", ActionMode::OnHold);
        actionMap.bindAdd("verlet.dt", "[", +0.001);
        actionMap.bindAdd("verlet.dt", "]", -0.001, ActionMode::OnHold);
        actionMap.bindAdd("verlet.dt", "MouseLeft", -0.001, ActionMode::OnHold);
    }

    void configure() {
        window->show();
        window->setTitle("LatticeLab");
    }

    void run() override {
        render->setup();
        while (!stopRequested()) {
            window->pollEvents();
            actionMap.tick(window->keyboard(), window->mouse());
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
    ActionMap actionMap;
    uint32_t fps;
    void print() {
        Logger::action("printer", "test");
    }
};