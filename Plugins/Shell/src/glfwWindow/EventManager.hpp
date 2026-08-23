#pragma once

#include <GLFW/glfw3.h>

class EventManager {
public:
    static void init(GLFWwindow* window);

    static void poll();
    static void frame(float deltaTime);

private:
    static GLFWwindow* window;
};