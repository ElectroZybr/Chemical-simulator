// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>

// Plugin dependences

// Sources
#include "WindowAPI.hpp"
#include "Window.hpp"
#include "Shell/src/glfwWindow/glfwWindow.hpp"

extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerAPI<WindowAPI>();
    reg.registerImpl<WindowAPI, glfwWindow>();
    reg.registerImpl<ServiceAPI, Window>();
    return true;
}

extern "C" void plugin_shutdown() {}
