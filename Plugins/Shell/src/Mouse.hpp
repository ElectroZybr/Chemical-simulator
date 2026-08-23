#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

namespace Input {

enum class MouseButton : uint8_t {
    Left = 0,
    Right,
    Middle,
    X1,
    X2,
    Count
};

inline constexpr std::size_t kMouseButtonCount =
    static_cast<std::size_t>(MouseButton::Count);

enum class ButtonAction : uint8_t { Press, Release, Repeat };

std::string_view mouseButtonToString(MouseButton b);
MouseButton mouseButtonFromString(std::string_view name);

struct MouseState {
    bool down[kMouseButtonCount]{};
    bool pressed[kMouseButtonCount]{};
    bool released[kMouseButtonCount]{};

    glm::vec2 pos{0.f, 0.f};       // текущая позиция (window coords)
    glm::vec2 delta{0.f, 0.f};     // сдвиг за кадр
    glm::vec2 scroll{0.f, 0.f};    // накопленный scroll за кадр
    glm::vec2 scrollDelta{0.f, 0.f};

    void beginFrame() {
        std::fill(std::begin(pressed), std::end(pressed), false);
        std::fill(std::begin(released), std::end(released), false);
        delta = {0.f, 0.f};
        scrollDelta = {0.f, 0.f};
        // scroll можно обнулять каждый кадр или копить — ниже обнуляем delta only
    }

    void onButton(MouseButton button, ButtonAction action) {
        const auto i = static_cast<std::size_t>(button);
        if (i >= kMouseButtonCount) return;

        if (action == ButtonAction::Press) {
            if (!down[i]) pressed[i] = true;
            down[i] = true;
        } else if (action == ButtonAction::Release) {
            if (down[i]) released[i] = true;
            down[i] = false;
        }
    }

    void onMove(float x, float y) {
        const glm::vec2 p{x, y};
        delta += p - pos;
        pos = p;
    }

    void onScroll(float dx, float dy) {
        scrollDelta += glm::vec2{dx, dy};
        scroll += glm::vec2{dx, dy};
    }

    bool isDown(MouseButton b) const {
        const auto i = static_cast<std::size_t>(b);
        return i < kMouseButtonCount && down[i];
    }
    bool wasPressed(MouseButton b) const {
        const auto i = static_cast<std::size_t>(b);
        return i < kMouseButtonCount && pressed[i];
    }
    bool wasReleased(MouseButton b) const {
        const auto i = static_cast<std::size_t>(b);
        return i < kMouseButtonCount && released[i];
    }
};

} // namespace Input