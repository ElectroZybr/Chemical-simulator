#include "Mouse.hpp"

#include <cctype>
#include <unordered_map>


namespace Input {

bool Mouse::down(std::string_view trigger) const {
    const auto button = buttonFromString(trigger);
    if (button == MouseButton::Count)
        return false;

    const auto i = static_cast<std::size_t>(button);
    return state_.down[i];
}

bool Mouse::pressed(std::string_view trigger) const {
    const auto button = buttonFromString(trigger);
    if (button == MouseButton::Count)
        return false;

    const auto i = static_cast<std::size_t>(button);
    return state_.pressed[i];
}

bool Mouse::released(std::string_view trigger) const {
    const auto button = buttonFromString(trigger);
    if (button == MouseButton::Count)
        return false;

    const auto i = static_cast<std::size_t>(button);
    return state_.released[i];
}

std::string_view Mouse::buttonToString(MouseButton button) {
    switch (button) {
    case MouseButton::Left:   return "MouseLeft";
    case MouseButton::Right:  return "MouseRight";
    case MouseButton::Middle: return "MouseMiddle";
    case MouseButton::X1:     return "MouseX1";
    case MouseButton::X2:     return "MouseX2";
    default:                  return "Unknown";
    }
}

MouseButton Mouse::buttonFromString(std::string_view name) {
    static const auto map = [] {
        std::unordered_map<std::string, MouseButton> m;

        auto put = [&](std::string s, MouseButton b) {
            for (char& c : s)
                c = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            m.emplace(std::move(s), b);
        };

        put("mouseleft", MouseButton::Left);
        put("left", MouseButton::Left);
        put("mouseright", MouseButton::Right);
        put("right", MouseButton::Right);
        put("mousemiddle", MouseButton::Middle);
        put("middle", MouseButton::Middle);
        put("mousex1", MouseButton::X1);
        put("mousex2", MouseButton::X2);

        return m;
    }();

    std::string key{name};
    for (char& c : key)
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));

    if (auto it = map.find(key); it != map.end())
        return it->second;

    return MouseButton::Count;
}
}