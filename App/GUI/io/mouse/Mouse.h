#pragma once
#include <memory>

#include <GLFW/glfw3.h>

#include "Lattice/Engine/math/Vec2.h"
#include "Rendering/BaseRenderer.h"

class Mouse {
    friend class EventManager;

public:
    static Vec2i getMousePos() {
        double x, y;
        glfwGetCursorPos(Mouse::window, &x, &y);
        return Vec2i(x, y);
    }

    static void init(GLFWwindow* w, std::unique_ptr<BaseRenderer>& renderer, class Interface& appInterface);

    static void onMouseButton(GLFWwindow* window, int button, int action, int mods);
    static void onMouseMove(GLFWwindow* window, double xpos, double ypos);
    static void onScroll(GLFWwindow* window, double xoffset, double yoffset);
    static void onFrame(float deltaTime);

    static void logMousePos();

private:
    static GLFWwindow* window;
    static std::unique_ptr<BaseRenderer>* renderer;
    static class Interface* appInterface;
    static GLFWmousebuttonfun imgui_mouse_callback;
    static GLFWcursorposfun imgui_cursor_pos_callback;
    static GLFWscrollfun imgui_scroll_callback;
};
