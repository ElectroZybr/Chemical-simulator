#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Input {

enum class Key : uint16_t {
    Unknown = 0,

    LeftShift, RightShift,
    LeftCtrl, RightCtrl,
    LeftAlt, RightAlt,
    LeftSuper, RightSuper,

    Escape, Enter, Tab, Backspace, Space,
    Left, Right, Up, Down,
    Insert, Delete, Home, End, PageUp, PageDown,

    Num0, Num1, Num2, Num3, Num4,
    Num5, Num6, Num7, Num8, Num9,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // OEM / punctuation (US layout names; physical keys via GLFW)
    Minus,         // -
    Equal,         // =
    LeftBracket,   // [
    RightBracket,  // ]
    Backslash,     // 
    Semicolon,     // ;
    Apostrophe,    // '
    GraveAccent,   // `
    Comma,         // ,
    Period,        // .
    Slash,         // /

    // Numpad
    Kp0, Kp1, Kp2, Kp3, Kp4,
    Kp5, Kp6, Kp7, Kp8, Kp9,
    KpDecimal, KpDivide, KpMultiply, KpSubtract, KpAdd,
    KpEnter, KpEqual,

    // Misc
    PrintScreen, ScrollLock, Pause,
    CapsLock, NumLock,
    Menu, // menu / app key

    Count
};

inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);

enum class KeyAction : uint8_t {
    Press,
    Release,
    Repeat
};

std::string_view keyToString(Key key);
Key keyFromString(std::string_view name);
bool isModifier(Key key);

struct KeyCombo {
    static constexpr uint8_t kMax = 4;

    std::array<Key, kMax> keys{};
    uint8_t count = 0;

    void clear() {
        keys.fill(Key::Unknown);
        count = 0;
    }

    void add(Key k);
    void normalize();

    bool contains(Key k) const {
        for (uint8_t i = 0; i < count; ++i)
            if (keys[i] == k) return true;
        return false;
    }

    bool operator==(const KeyCombo& o) const {
        if (count != o.count) return false;
        for (uint8_t i = 0; i < count; ++i)
            if (keys[i] != o.keys[i]) return false;
        return true;
    }
};

struct KeyComboHash {
    std::size_t operator()(const KeyCombo& c) const noexcept {
        std::size_t h = c.count;
        for (uint8_t i = 0; i < c.count; ++i)
            h ^= static_cast<std::size_t>(c.keys[i]) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

std::optional<KeyCombo> parseCombo(std::string_view text);
std::string toString(const KeyCombo& c);

struct KeyboardState {
    bool down[kKeyCount]{};
    bool pressed[kKeyCount]{};
    bool released[kKeyCount]{};

    void beginFrame();
    void onKey(Key key, KeyAction action);

    bool isDown(Key k) const;
    bool wasPressed(Key k) const;
    bool wasReleased(Key k) const;

    KeyCombo currentDownCombo() const;
};

bool comboDown(const KeyCombo& combo, const KeyboardState& kb);
bool comboPressed(const KeyCombo& combo, const KeyboardState& kb, bool wasDownLastFrame);

} // namespace Input