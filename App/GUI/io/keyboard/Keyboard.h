#pragma once
#include <memory>

#include <GLFW/glfw3.h>

#include "Rendering/BaseRenderer.h"

class Keyboard {
    friend class EventManager;

public:
    static void init(GLFWwindow* window, std::unique_ptr<BaseRenderer>& r, class Interface& appInterface);

    static bool isPressed(int key);

    static void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void onFrame(float deltaTime);

private:
    static GLFWwindow* window;
    static std::unique_ptr<BaseRenderer>* render;
    static class Interface* appInterface;
    static GLFWkeyfun imgui_key_callback;
};
