// Kernel dependences
#include <Lattice/Kernel/Plugin.hpp>

// Plugin dependences

// Sources
#include "glfwWindow/glfwWindow.hpp"
#include "InputAPI.hpp"
#include "KeybindLoader.hpp"
#include "LoaderAPI.hpp"
#include "WindowAPI.hpp"
#include "Window.hpp"
#include "Mouse.hpp"
#include "Keyboard.hpp"


extern "C" bool plugin_register(Lattice::Registry& reg) {
    reg.registerAPI<WindowAPI>();
    reg.registerImpl<WindowAPI, glfwWindow>();
    reg.registerImpl<ServiceAPI, Window>();

    reg.registerImpl<InputAPI, Input::Keyboard>();
    reg.registerImpl<InputAPI, Input::Mouse>();

    reg.registerImpl<LoaderAPI, KeybindsLoader>();
    return true;
}

extern "C" void plugin_shutdown() {}
