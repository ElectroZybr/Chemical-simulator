#include "EventManager.h"

#include <imgui.h>

#include "GUI/io/keyboard/Keyboard.h"
#include "GUI/io/mouse/Mouse.h"
#include "GUI/io/window_events/WindowEvents.h"

GLFWwindow* EventManager::window = nullptr;
std::unique_ptr<BaseRenderer>* EventManager::renderer = nullptr;

void EventManager::init(GLFWwindow* w, std::unique_ptr<BaseRenderer>& r, Interface& appInterface) {
    window = w;
    renderer = &r;

    WindowEvents::init(w, r, appInterface);
    Keyboard::init(w, r, appInterface);
    Mouse::init(w, r, appInterface);
}

void EventManager::poll() { glfwPollEvents(); }

void EventManager::frame(float deltaTime) {
    Keyboard::onFrame(deltaTime);
    Mouse::onFrame(deltaTime);
}
