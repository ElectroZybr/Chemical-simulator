#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "InputAPI.hpp"


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

    Minus, Equal,
    LeftBracket, RightBracket,
    Backslash, Semicolon, Apostrophe,
    GraveAccent, Comma, Period, Slash,

    Kp0, Kp1, Kp2, Kp3, Kp4,
    Kp5, Kp6, Kp7, Kp8, Kp9,
    KpDecimal, KpDivide, KpMultiply, KpSubtract, KpAdd,
    KpEnter, KpEqual,

    PrintScreen, ScrollLock, Pause,
    CapsLock, NumLock, Menu,

    Count
};

inline constexpr std::size_t kKeyCount =
    static_cast<std::size_t>(Key::Count);

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

    void clear();
    void add(Key key);
    void normalize();

    bool contains(Key key) const {
        for (uint8_t i = 0; i < count; ++i)
            if (keys[i] == key)
                return true;
        return false;
    }

    bool operator==(const KeyCombo& other) const;
};

struct KeyComboHash {
    std::size_t operator()(const KeyCombo& combo) const noexcept;
};

std::optional<KeyCombo> parseCombo(std::string_view text);
std::string toString(const KeyCombo& combo);

struct KeyboardState {
    bool down[kKeyCount]{};
    bool pressed[kKeyCount]{};
    bool released[kKeyCount]{};

    void beginFrame();
    void onKey(Key key, KeyAction action);

    bool isDown(Key key) const;
    bool wasPressed(Key key) const;
    bool wasReleased(Key key) const;
};


class Keyboard final : public InputAPI {
public:
    void beginFrame();
    void onKey(Key key, KeyAction action);

    bool down(std::string_view trigger) const override;
    bool pressed(std::string_view trigger) const override;
    bool released(std::string_view trigger) const override;

private:
    KeyboardState state_;

    bool comboDown(const KeyCombo& combo) const;
    bool comboPressed(const KeyCombo& combo) const;
    bool comboReleased(const KeyCombo& combo) const;
};

} // namespace Input