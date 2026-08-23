#include "Keyboard.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Input {
namespace {

void toLowerInPlace(std::string& s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

Key parseToken(std::string_view t) {
    std::string s(t);
    toLowerInPlace(s);
    if (s == "ctrl" || s == "control") return Key::LeftCtrl;
    if (s == "shift") return Key::LeftShift;
    if (s == "alt") return Key::LeftAlt;
    if (s == "super" || s == "meta" || s == "cmd") return Key::LeftSuper;
    return keyFromString(s);
}

bool modFamilyDown(Key mod, const KeyboardState& kb) {
    switch (mod) {
    case Key::LeftCtrl: case Key::RightCtrl:
        return kb.isDown(Key::LeftCtrl) || kb.isDown(Key::RightCtrl);
    case Key::LeftShift: case Key::RightShift:
        return kb.isDown(Key::LeftShift) || kb.isDown(Key::RightShift);
    case Key::LeftAlt: case Key::RightAlt:
        return kb.isDown(Key::LeftAlt) || kb.isDown(Key::RightAlt);
    case Key::LeftSuper: case Key::RightSuper:
        return kb.isDown(Key::LeftSuper) || kb.isDown(Key::RightSuper);
    default:
        return kb.isDown(mod);
    }
}

} // namespace

void KeyCombo::add(Key k) {
    if (k == Key::Unknown || count >= kMax) return;
    for (uint8_t i = 0; i < count; ++i)
        if (keys[i] == k) return;
    keys[count++] = k;
    normalize();
}

void KeyCombo::normalize() {
    if (count <= 1) return;
    std::sort(keys.begin(), keys.begin() + count,
              [](Key a, Key b) {
                  return static_cast<uint16_t>(a) < static_cast<uint16_t>(b);
              });
    uint8_t w = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (w == 0 || keys[i] != keys[w - 1])
            keys[w++] = keys[i];
    }
    for (uint8_t i = w; i < kMax; ++i)
        keys[i] = Key::Unknown;
    count = w;
}

bool isModifier(Key key) {
    switch (key) {
    case Key::LeftShift: case Key::RightShift:
    case Key::LeftCtrl:  case Key::RightCtrl:
    case Key::LeftAlt:   case Key::RightAlt:
    case Key::LeftSuper: case Key::RightSuper:
        return true;
    default:
        return false;
    }
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
            toLowerInPlace(k);
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
    toLowerInPlace(key);

    if (auto it = map.find(key); it != map.end())
        return it->second;
    return Key::Unknown;
}

std::optional<KeyCombo> parseCombo(std::string_view text) {
    KeyCombo combo;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t plus = text.find('+', start);
        auto token = text.substr(
            start,
            plus == std::string_view::npos ? std::string_view::npos : plus - start);

        while (!token.empty() && token.front() == ' ') token.remove_prefix(1);
        while (!token.empty() && token.back() == ' ') token.remove_suffix(1);

        if (!token.empty()) {
            const Key k = parseToken(token);
            if (k == Key::Unknown) return std::nullopt;
            combo.add(k);
        }

        if (plus == std::string_view::npos) break;
        start = plus + 1;
    }
    if (combo.count == 0) return std::nullopt;
    return combo;
}

std::string toString(const KeyCombo& c) {
    std::string out;
    for (uint8_t i = 0; i < c.count; ++i) {
        if (i) out += '+';
        out += keyToString(c.keys[i]);
    }
    return out;
}

void KeyboardState::beginFrame() {
    std::fill(std::begin(pressed), std::end(pressed), false);
    std::fill(std::begin(released), std::end(released), false);
}

void KeyboardState::onKey(Key key, KeyAction action) {
    if (key == Key::Unknown) return;
    const std::size_t i = static_cast<std::size_t>(key);
    if (i >= kKeyCount) return;

    if (action == KeyAction::Press) {
        if (!down[i]) pressed[i] = true;
        down[i] = true;
    } else if (action == KeyAction::Release) {
        if (down[i]) released[i] = true;
        down[i] = false;
    }
}

bool KeyboardState::isDown(Key k) const {
    const auto i = static_cast<std::size_t>(k);
    return i < kKeyCount && down[i];
}

bool KeyboardState::wasPressed(Key k) const {
    const auto i = static_cast<std::size_t>(k);
    return i < kKeyCount && pressed[i];
}

bool KeyboardState::wasReleased(Key k) const {
    const auto i = static_cast<std::size_t>(k);
    return i < kKeyCount && released[i];
}

KeyCombo KeyboardState::currentDownCombo() const {
    KeyCombo c;
    for (std::size_t i = 1; i < kKeyCount && c.count < KeyCombo::kMax; ++i) {
        if (down[i])
            c.add(static_cast<Key>(i));
    }
    return c;
}

bool comboDown(const KeyCombo& combo, const KeyboardState& kb) {
    if (combo.count == 0) return false;
    for (uint8_t i = 0; i < combo.count; ++i) {
        if (!modFamilyDown(combo.keys[i], kb))
            return false;
    }
    return true;
}

bool comboPressed(const KeyCombo& combo, const KeyboardState& kb, bool wasDownLastFrame) {
    return comboDown(combo, kb) && !wasDownLastFrame;
}

} // namespace Input