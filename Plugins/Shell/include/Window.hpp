#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

#include "Render.hpp"

#include "WindowAPI.hpp"
#include "Render/include/Render.hpp"
#include "Shell/src/glfwWindow/glfwWindow.hpp"

class Window final : public ServiceAPI {
public:
    explicit Window(Lattice::Components& branch) {
        settings = branch.require<Lattice::Settings>();
        window = branch.use<WindowAPI, glfwWindow>();
        render = branch.add<Render>();
        Logger::info("Window", "window created");
    }

    void configure() {
        window->show();
        window->setTitle("LatticeLab");
    }

    void run() override {
        render->setup();
        while (!stopRequested()) {
            window->pollEvents();
            if (window->shouldClose()) {
                requestStop();
                break;
            }
            render->frame();
            Logger::info("Window", "looping");
        }
    }

    ~Window() {
        stop();
        Logger::info("Window", "destroying object");
    }

private:
    Lattice::Settings* settings = nullptr;
    Lattice::Slot<WindowAPI> window;
    Render* render = nullptr;
    uint32_t fps;
};