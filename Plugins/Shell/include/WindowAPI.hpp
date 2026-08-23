#pragma once

#include <string_view>
#include <glm/vec2.hpp>
#include "GPU/include/NativeWindow.hpp"

#include "NativeWindow.hpp"


namespace Input { struct KeyboardState; }

class WindowAPI {
public:
    struct State {
        std::string_view name = "default";
        bool fullscreen = false;
        bool maximized = false;
        int monitorIndex = 0;
        int x = 160;
        int y = 120;
        int width = 1280;
        int height = 720;
    };

    virtual ~WindowAPI() = default;

    // lifecycle
    virtual bool shouldClose() const = 0;
    virtual void requestClose() = 0;

    // pump
    virtual void pollEvents() = 0;
    virtual const Input::KeyboardState& keyboard() const = 0;

    // geometry
    virtual glm::vec2 windowSize() const = 0;
    virtual glm::vec2 framebufferSize() const = 0;
    virtual float contentScale() const = 0;
    virtual bool fullscreen() const = 0;
    virtual void setFullscreen(bool enabled) = 0;

    // for GPU surface
    virtual NativeWindow native() const = 0;

    // optional
    virtual void show() = 0;
    virtual void setTitle(std::string_view title) = 0;
};