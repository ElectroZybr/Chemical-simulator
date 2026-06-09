#include "WindowEvents.h"

#include <glm/vec2.hpp>
#include "GUI/interface/interface.h"
#include "Rendering/BaseRenderer.h"
#include "Rendering/backend/WGPUContext.h"

GLFWwindow* WindowEvents::window = nullptr;
std::unique_ptr<BaseRenderer>* WindowEvents::renderer = nullptr;
Interface* WindowEvents::appInterface = nullptr;

void WindowEvents::init(GLFWwindow* w, std::unique_ptr<BaseRenderer>& r, Interface& appInterface) {
    window = w;
    renderer = &r;
    WindowEvents::appInterface = &appInterface;

    glfwSetWindowCloseCallback(window, windowCloseCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    syncFramebufferSize(width, height, false);
}

void WindowEvents::windowCloseCallback(GLFWwindow* window) { glfwSetWindowShouldClose(window, GLFW_TRUE); }

void WindowEvents::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    syncFramebufferSize(width, height, true);
}

void WindowEvents::syncFramebufferSize(int width, int height, bool updateInterface) {
    if (width <= 0 || height <= 0) {
        return;
    }

    (*renderer)->camera.setScreenSize(glm::vec2(static_cast<float>(width), static_cast<float>(height)));
    WGPUContext::instance().resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}
