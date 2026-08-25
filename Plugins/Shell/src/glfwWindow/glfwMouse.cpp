#include "glfwMouse.hpp"
#include <GLFW/glfw3.h>

namespace Input {

MouseButton mouseButtonFromGlfw(int button) {
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:   return MouseButton::Left;
    case GLFW_MOUSE_BUTTON_RIGHT:  return MouseButton::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
    case GLFW_MOUSE_BUTTON_4:      return MouseButton::X1;
    case GLFW_MOUSE_BUTTON_5:      return MouseButton::X2;
    default:                       return MouseButton::Count;
    }
}

ButtonAction buttonActionFromGlfw(int action) {
    switch (action) {
    case GLFW_PRESS:   return ButtonAction::Press;
    case GLFW_RELEASE: return ButtonAction::Release;
    default:           return ButtonAction::Repeat;
    }
}

}