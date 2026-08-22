#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

#include "Render.hpp"

#include "WindowAPI.hpp"
#include "Render/include/Render.hpp"
#include "Window/src/glfwWindow/glfwWindow.hpp"

class Window final : public ServiceAPI {
public:
    explicit Window(Lattice::Components& branch) {
        settings = branch.require<Lattice::Settings>();
        window = branch.addInterfaceSlot<WindowAPI>();
        branch.useInterface<WindowAPI, glfwWindow>();
        render = branch.addComponent<Render>();
        Logger::info("Window", "window created");
    }

    void configure() {
        window->show();
        window->setTitle("LatticeLab");
    }

    void run() override {
        while (!stopRequested()) {
            window->pollEvents();
            if (window->shouldClose()) {
                requestStop();
                break;
            }
            Logger::info("Window", "looping");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    ~Window() {
        stop();
        Logger::info("Window", "destroying object");
    }

private:
    Lattice::Component<Lattice::Settings> settings;
    Lattice::Component<WindowAPI> window;
    Lattice::Component<Render> render;
    uint32_t fps;
};