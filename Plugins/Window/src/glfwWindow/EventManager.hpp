#pragma once
#include <memory>

#include <GLFW/glfw3.h>

class BaseRenderer;
class Interface;

class EventManager {
public:
    static void init(GLFWwindow* window, std::unique_ptr<BaseRenderer>& renderer, Interface& appInterface);

    static void poll();
    static void frame(float deltaTime);

private:
    static GLFWwindow* window;
    static std::unique_ptr<BaseRenderer>* renderer;
};