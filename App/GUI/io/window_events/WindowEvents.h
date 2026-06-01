#pragma once
#include <memory>

#include <GLFW/glfw3.h>

class BaseRenderer;
class Interface;

class WindowEvents {
    friend class EventManager;

public:
    static void init(GLFWwindow* w, std::unique_ptr<BaseRenderer>& renderer, Interface& appInterface);

    static void windowCloseCallback(GLFWwindow* window);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

private:
    static void syncFramebufferSize(int width, int height, bool updateInterface);

    static GLFWwindow* window;
    static std::unique_ptr<BaseRenderer>* renderer;
    static Interface* appInterface;
};
