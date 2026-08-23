#pragma once

#include <GLFW/glfw3.h>
#include <string_view>

#include "WindowAPI.hpp"
#include "glfwKeyboard.hpp"

class glfwWindow final : public WindowAPI {
public:
    explicit glfwWindow(const State& state = State{});
    ~glfwWindow() override;

    glfwWindow(const glfwWindow&) = delete;
    glfwWindow& operator=(const glfwWindow&) = delete;
    glfwWindow(glfwWindow&&) noexcept;
    glfwWindow& operator=(glfwWindow&&) noexcept;

    // WindowAPI
    bool shouldClose() const override;
    void requestClose() override;
    void pollEvents() override;

    glm::vec2 windowSize() const override;
    glm::vec2 framebufferSize() const override;
    float contentScale() const override;
    bool fullscreen() const override;
    void setFullscreen(bool enabled) override;

    NativeWindow native() const override;
    void show() override;
    void setTitle(std::string_view title) override;

    const Input::KeyboardState& keyboard() const override { return keyboard_; }

private:
    static void posCallback(GLFWwindow* w, int x, int y);
    static void sizeCallback(GLFWwindow* w, int width, int height);
    static void maximizeCallback(GLFWwindow* w, int maximized);
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);

    void onPos(int x, int y);
    void onSize(int width, int height);
    void onMaximize(int maximized);

    void syncFromWindow();
    GLFWmonitor* currentMonitor() const;
    GLFWmonitor* monitorByIndex(int index) const;
    int monitorIndex(GLFWmonitor* monitor) const;
    void applyWindowed();
    void applyFullscreen(GLFWmonitor* monitor);

    Input::KeyboardState keyboard_;
    GLFWwindow* window_ = nullptr;
    State state_{};
};