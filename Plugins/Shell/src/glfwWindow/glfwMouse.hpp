#pragma once

#include "Mouse.hpp"

namespace Input {

MouseButton mouseButtonFromGlfw(int button);
ButtonAction buttonActionFromGlfw(int action);

}