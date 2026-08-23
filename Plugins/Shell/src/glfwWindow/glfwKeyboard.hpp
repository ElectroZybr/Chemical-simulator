// GlfwKeyMap.hpp
#pragma once
#include "Keyboard.hpp"

namespace Input {
Key keyFromGlfw(int glfwKey);
KeyAction keyActionFromGlfw(int glfwAction);
}