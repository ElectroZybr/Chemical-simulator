#include "Keyboard.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <Lattice/Tools/Logger.hpp>

namespace Input {
namespace {

void lower(std::string& s) {
    for (char& c : s)
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
}

Key parseToken(std::string_view token) {
    std::string s(token);
    lower(s);

    if (s == "ctrl" || s == "control") return Key::LeftCtrl;
    if (s == "shift") return Key::LeftShift;
    if (s == "alt") return Key::LeftAlt;
    if (s == "super" || s == "meta" || s == "cmd")
        return Key::LeftSuper;

    return keyFromString(s);
}

bool modifierDown(Key key, const KeyboardState& state) {
    switch (key) {
    case Key::LeftCtrl:
    case Key::RightCtrl:
        return state.isDown(Key::LeftCtrl) ||
               state.isDown(Key::RightCtrl);

    case Key::LeftShift:
    case Key::RightShift:
        return state.isDown(Key::LeftShift) ||
               state.isDown(Key::RightShift);

    case Key::LeftAlt:
    case Key::RightAlt:
        return state.isDown(Key::LeftAlt) ||
               state.isDown(Key::RightAlt);

    case Key::LeftSuper:
    case Key::RightSuper:
        return state.isDown(Key::LeftSuper) ||
               state.isDown(Key::RightSuper);

    default:
        return state.isDown(key);
    }
}

} // namespace

void KeyCombo::clear() {
    keys.fill(Key::Unknown);
    count = 0;
}

void KeyCombo::add(Key key) {
    if (key == Key::Unknown || count >= kMax || contains(key))
        return;

    keys[count++] = key;
    normalize();
}

void KeyCombo::normalize() {
    std::sort(keys.begin(), keys.begin() + count,
        [](Key a, Key b) {
            return static_cast<uint16_t>(a) <
                   static_cast<uint16_t>(b);
        });

    uint8_t unique = 0;

    for (uint8_t i = 0; i < count; ++i) {
        if (unique == 0 || keys[i] != keys[unique - 1])
            keys[unique++] = keys[i];
    }

    for (uint8_t i = unique; i < kMax; ++i)
        keys[i] = Key::Unknown;

    count = unique;
}

bool KeyCombo::operator==(const KeyCombo& other) const {
    if (count != other.count)
        return false;

    for (uint8_t i = 0; i < count; ++i)
        if (keys[i] != other.keys[i])
            return false;

    return true;
}

std::size_t KeyComboHash::operator()(const KeyCombo& combo) const noexcept {
    std::size_t hash = combo.count;

    for (uint8_t i = 0; i < combo.count; ++i)
        hash ^= static_cast<std::size_t>(combo.keys[i]) +
                0x9e3779b9u + (hash << 6) + (hash >> 2);

    return hash;
}

std::optional<KeyCombo> parseCombo(std::string_view text) {
    KeyCombo combo;

    std::size_t begin = 0;

    while (begin <= text.size()) {
        const auto end = text.find('+', begin);

        auto token = text.substr(
            begin,
            end == std::string_view::npos
                ? std::string_view::npos
                : end - begin
        );

        while (!token.empty() && token.front() == ' ')
            token.remove_prefix(1);

        while (!token.empty() && token.back() == ' ')
            token.remove_suffix(1);

        if (!token.empty()) {
            const Key key = parseToken(token);

            if (key == Key::Unknown)
                return std::nullopt;

            combo.add(key);
        }

        if (end == std::string_view::npos)
            break;

        begin = end + 1;
    }

    if (combo.count == 0)
        return std::nullopt;

    return combo;
}

std::string_view keyToString(Key key) {
    switch (key) {
    case Key::A: return "A"; case Key::B: return "B"; case Key::C: return "C";
    case Key::D: return "D"; case Key::E: return "E"; case Key::F: return "F";
    case Key::G: return "G"; case Key::H: return "H"; case Key::I: return "I";
    case Key::J: return "J"; case Key::K: return "K"; case Key::L: return "L";
    case Key::M: return "M"; case Key::N: return "N"; case Key::O: return "O";
    case Key::P: return "P"; case Key::Q: return "Q"; case Key::R: return "R";
    case Key::S: return "S"; case Key::T: return "T"; case Key::U: return "U";
    case Key::V: return "V"; case Key::W: return "W"; case Key::X: return "X";
    case Key::Y: return "Y"; case Key::Z: return "Z";

    case Key::Num0: return "0"; case Key::Num1: return "1"; case Key::Num2: return "2";
    case Key::Num3: return "3"; case Key::Num4: return "4"; case Key::Num5: return "5";
    case Key::Num6: return "6"; case Key::Num7: return "7"; case Key::Num8: return "8";
    case Key::Num9: return "9";

    case Key::Escape: return "Escape"; case Key::Enter: return "Enter";
    case Key::Tab: return "Tab"; case Key::Backspace: return "Backspace";
    case Key::Space: return "Space";
    case Key::Left: return "Left"; case Key::Right: return "Right";
    case Key::Up: return "Up"; case Key::Down: return "Down";
    case Key::Insert: return "Insert"; case Key::Delete: return "Delete";
    case Key::Home: return "Home"; case Key::End: return "End";
    case Key::PageUp: return "PageUp"; case Key::PageDown: return "PageDown";

    case Key::LeftShift: return "LeftShift"; case Key::RightShift: return "RightShift";
    case Key::LeftCtrl: return "LeftCtrl"; case Key::RightCtrl: return "RightCtrl";
    case Key::LeftAlt: return "LeftAlt"; case Key::RightAlt: return "RightAlt";
    case Key::LeftSuper: return "LeftSuper"; case Key::RightSuper: return "RightSuper";

    case Key::F1: return "F1"; case Key::F2: return "F2"; case Key::F3: return "F3";
    case Key::F4: return "F4"; case Key::F5: return "F5"; case Key::F6: return "F6";
    case Key::F7: return "F7"; case Key::F8: return "F8"; case Key::F9: return "F9";
    case Key::F10: return "F10"; case Key::F11: return "F11"; case Key::F12: return "F12";

    case Key::Minus: return "Minus";
    case Key::Equal: return "Equal";
    case Key::LeftBracket: return "LeftBracket";
    case Key::RightBracket: return "RightBracket";
    case Key::Backslash: return "Backslash";
    case Key::Semicolon: return "Semicolon";
    case Key::Apostrophe: return "Apostrophe";
    case Key::GraveAccent: return "GraveAccent";
    case Key::Comma: return "Comma";
    case Key::Period: return "Period";
    case Key::Slash: return "Slash";
        
    default: return "Unknown";
    }
}

Key keyFromString(std::string_view name) {
    static const auto map = [] {
        std::unordered_map<std::string, Key> m;

        auto put = [&](std::string k, Key v) {
            lower(k);
            m.emplace(std::move(k), v);
        };

        for (std::size_t i = 1; i < kKeyCount; ++i) {
            const Key key = static_cast<Key>(i);
            put(std::string(keyToString(key)), key);
        }

        // aliases (already lower)
        put("ctrl", Key::LeftCtrl);
        put("control", Key::LeftCtrl);
        put("shift", Key::LeftShift);
        put("alt", Key::LeftAlt);
        put("super", Key::LeftSuper);
        put("meta", Key::LeftSuper);
        put("cmd", Key::LeftSuper);

        put("-", Key::Minus);
        put("=", Key::Equal);
        put("[", Key::LeftBracket);
        put("]", Key::RightBracket);
        put("\\", Key::Backslash);
        put(";", Key::Semicolon);
        put("'", Key::Apostrophe);
        put("`", Key::GraveAccent);
        put(",", Key::Comma);
        put(".", Key::Period);
        put("/", Key::Slash);

        return m;
    }();

    std::string key{name};
    lower(key);

    if (auto it = map.find(key); it != map.end())
        return it->second;
    return Key::Unknown;
}

std::string toString(const KeyCombo& combo) {
    std::string result;

    for (uint8_t i = 0; i < combo.count; ++i) {
        if (i)
            result += '+';

        result += keyToString(combo.keys[i]);
    }
    return result;
}

void KeyboardState::beginFrame() {
    std::fill(std::begin(pressed), std::end(pressed), false);
    std::fill(std::begin(released), std::end(released), false);
}

void KeyboardState::onKey(Key key, KeyAction action) {
    if (key == Key::Unknown)
        return;

    const auto index = static_cast<std::size_t>(key);

    if (index >= kKeyCount)
        return;

    switch (action) {
    case KeyAction::Press:
        if (!down[index])
            pressed[index] = true;
        down[index] = true;
        break;

    case KeyAction::Release:
        if (down[index])
            released[index] = true;
        down[index] = false;
        break;

    case KeyAction::Repeat:
        break;
    }
}

bool KeyboardState::isDown(Key key) const {
    const auto index = static_cast<std::size_t>(key);
    return index < kKeyCount && down[index];
}

bool KeyboardState::wasPressed(Key key) const {
    const auto index = static_cast<std::size_t>(key);
    return index < kKeyCount && pressed[index];
}

bool KeyboardState::wasReleased(Key key) const {
    const auto index = static_cast<std::size_t>(key);
    return index < kKeyCount && released[index];
}

void Keyboard::beginFrame() {
    state_.beginFrame();
}

void Keyboard::onKey(Key key, KeyAction action) {
    state_.onKey(key, action);
        // Logger::info("Keyboard", "onKey {} action={} down={}",
        //     keyToString(key),
        //     static_cast<int>(action),
        //     state_.isDown(key));
}

bool Keyboard::comboDown(const KeyCombo& combo) const {
    if (combo.count == 0)
        return false;

    for (uint8_t i = 0; i < combo.count; ++i) {
        if (!modifierDown(combo.keys[i], state_))
            return false;
    }

    return true;
}

bool Keyboard::comboPressed(const KeyCombo& combo) const {
    if (!comboDown(combo))
        return false;

    for (uint8_t i = 0; i < combo.count; ++i) {
        if (state_.wasPressed(combo.keys[i]))
            return true;
    }

    return false;
}

bool Keyboard::comboReleased(const KeyCombo& combo) const {
    for (uint8_t i = 0; i < combo.count; ++i) {
        if (state_.wasReleased(combo.keys[i]))
            return true;
    }

    return false;
}

bool Keyboard::down(std::string_view trigger) const {
    const auto combo = parseCombo(trigger);
    return combo && comboDown(*combo);
}

bool Keyboard::pressed(std::string_view trigger) const {
    const auto combo = parseCombo(trigger);
    return combo && comboPressed(*combo);
}

bool Keyboard::released(std::string_view trigger) const {
    const auto combo = parseCombo(trigger);
    return combo && comboReleased(*combo);
}

} // namespace Input