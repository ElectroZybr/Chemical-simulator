#include "Keyboard.hpp"
#include <GLFW/glfw3.h>

namespace Input {

Key keyFromGlfw(int k) {
    switch (k) {
    // Letters
    case GLFW_KEY_A: return Key::A;
    case GLFW_KEY_B: return Key::B;
    case GLFW_KEY_C: return Key::C;
    case GLFW_KEY_D: return Key::D;
    case GLFW_KEY_E: return Key::E;
    case GLFW_KEY_F: return Key::F;
    case GLFW_KEY_G: return Key::G;
    case GLFW_KEY_H: return Key::H;
    case GLFW_KEY_I: return Key::I;
    case GLFW_KEY_J: return Key::J;
    case GLFW_KEY_K: return Key::K;
    case GLFW_KEY_L: return Key::L;
    case GLFW_KEY_M: return Key::M;
    case GLFW_KEY_N: return Key::N;
    case GLFW_KEY_O: return Key::O;
    case GLFW_KEY_P: return Key::P;
    case GLFW_KEY_Q: return Key::Q;
    case GLFW_KEY_R: return Key::R;
    case GLFW_KEY_S: return Key::S;
    case GLFW_KEY_T: return Key::T;
    case GLFW_KEY_U: return Key::U;
    case GLFW_KEY_V: return Key::V;
    case GLFW_KEY_W: return Key::W;
    case GLFW_KEY_X: return Key::X;
    case GLFW_KEY_Y: return Key::Y;
    case GLFW_KEY_Z: return Key::Z;

    // Digits
    case GLFW_KEY_0: return Key::Num0;
    case GLFW_KEY_1: return Key::Num1;
    case GLFW_KEY_2: return Key::Num2;
    case GLFW_KEY_3: return Key::Num3;
    case GLFW_KEY_4: return Key::Num4;
    case GLFW_KEY_5: return Key::Num5;
    case GLFW_KEY_6: return Key::Num6;
    case GLFW_KEY_7: return Key::Num7;
    case GLFW_KEY_8: return Key::Num8;
    case GLFW_KEY_9: return Key::Num9;

    // Special
    case GLFW_KEY_ESCAPE:    return Key::Escape;
    case GLFW_KEY_ENTER:     return Key::Enter;
    case GLFW_KEY_TAB:       return Key::Tab;
    case GLFW_KEY_BACKSPACE: return Key::Backspace;
    case GLFW_KEY_SPACE:     return Key::Space;
    case GLFW_KEY_LEFT:      return Key::Left;
    case GLFW_KEY_RIGHT:     return Key::Right;
    case GLFW_KEY_UP:        return Key::Up;
    case GLFW_KEY_DOWN:      return Key::Down;
    case GLFW_KEY_INSERT:    return Key::Insert;
    case GLFW_KEY_DELETE:    return Key::Delete;
    case GLFW_KEY_HOME:      return Key::Home;
    case GLFW_KEY_END:       return Key::End;
    case GLFW_KEY_PAGE_UP:   return Key::PageUp;
    case GLFW_KEY_PAGE_DOWN: return Key::PageDown;

    // Modifiers
    case GLFW_KEY_LEFT_SHIFT:    return Key::LeftShift;
    case GLFW_KEY_RIGHT_SHIFT:   return Key::RightShift;
    case GLFW_KEY_LEFT_CONTROL:  return Key::LeftCtrl;
    case GLFW_KEY_RIGHT_CONTROL: return Key::RightCtrl;
    case GLFW_KEY_LEFT_ALT:      return Key::LeftAlt;
    case GLFW_KEY_RIGHT_ALT:     return Key::RightAlt;
    case GLFW_KEY_LEFT_SUPER:    return Key::LeftSuper;
    case GLFW_KEY_RIGHT_SUPER:   return Key::RightSuper;

    // Function
    case GLFW_KEY_F1:  return Key::F1;
    case GLFW_KEY_F2:  return Key::F2;
    case GLFW_KEY_F3:  return Key::F3;
    case GLFW_KEY_F4:  return Key::F4;
    case GLFW_KEY_F5:  return Key::F5;
    case GLFW_KEY_F6:  return Key::F6;
    case GLFW_KEY_F7:  return Key::F7;
    case GLFW_KEY_F8:  return Key::F8;
    case GLFW_KEY_F9:  return Key::F9;
    case GLFW_KEY_F10: return Key::F10;
    case GLFW_KEY_F11: return Key::F11;
    case GLFW_KEY_F12: return Key::F12;

    case GLFW_KEY_MINUS: return Key::Minus;
    case GLFW_KEY_EQUAL: return Key::Equal;
    case GLFW_KEY_LEFT_BRACKET: return Key::LeftBracket;
    case GLFW_KEY_RIGHT_BRACKET: return Key::RightBracket;
    case GLFW_KEY_BACKSLASH: return Key::Backslash;
    case GLFW_KEY_SEMICOLON: return Key::Semicolon;
    case GLFW_KEY_APOSTROPHE: return Key::Apostrophe;
    case GLFW_KEY_GRAVE_ACCENT: return Key::GraveAccent;
    case GLFW_KEY_COMMA: return Key::Comma;
    case GLFW_KEY_PERIOD: return Key::Period;
    case GLFW_KEY_SLASH: return Key::Slash;

    case GLFW_KEY_KP_0: return Key::Kp0;
    case GLFW_KEY_KP_1: return Key::Kp1;
    case GLFW_KEY_KP_2: return Key::Kp2;
    case GLFW_KEY_KP_3: return Key::Kp3;
    case GLFW_KEY_KP_4: return Key::Kp4;
    case GLFW_KEY_KP_5: return Key::Kp5;
    case GLFW_KEY_KP_6: return Key::Kp6;
    case GLFW_KEY_KP_7: return Key::Kp7;
    case GLFW_KEY_KP_8: return Key::Kp8;
    case GLFW_KEY_KP_9: return Key::Kp9;
    case GLFW_KEY_KP_DECIMAL: return Key::KpDecimal;
    case GLFW_KEY_KP_DIVIDE: return Key::KpDivide;
    case GLFW_KEY_KP_MULTIPLY: return Key::KpMultiply;
    case GLFW_KEY_KP_SUBTRACT: return Key::KpSubtract;
    case GLFW_KEY_KP_ADD: return Key::KpAdd;
    case GLFW_KEY_KP_ENTER: return Key::KpEnter;
    case GLFW_KEY_KP_EQUAL: return Key::KpEqual;

    case GLFW_KEY_PRINT_SCREEN: return Key::PrintScreen;
    case GLFW_KEY_SCROLL_LOCK: return Key::ScrollLock;
    case GLFW_KEY_PAUSE: return Key::Pause;
    case GLFW_KEY_CAPS_LOCK: return Key::CapsLock;
    case GLFW_KEY_NUM_LOCK: return Key::NumLock;
    case GLFW_KEY_MENU: return Key::Menu;

    default:
        return Key::Unknown;
    }
}

KeyAction keyActionFromGlfw(int a) {
    if (a == GLFW_PRESS) return KeyAction::Press;
    if (a == GLFW_RELEASE) return KeyAction::Release;
    return KeyAction::Repeat;
}
}