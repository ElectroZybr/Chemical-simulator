#pragma once

#include <GLFW/glfw3.h>
#include <string_view>

#include <Lattice/Kernel/Node.hpp>

#include "WindowAPI.hpp"
#include "Keyboard.hpp"
#include "Mouse.hpp"

class glfwWindow final : public WindowAPI {
public:
    explicit glfwWindow(Lattice::Node& branch);
    void configure(Lattice::Node& branch);

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

    const Ref<Input::Keyboard> keyboard() const override { return keyboard_; }
    const Ref<Input::Mouse> mouse() const override { return mouse_; }

private:
    static constexpr std::string_view tag = "glfwWindow";
    static void posCallback(GLFWwindow* w, int x, int y);
    static void sizeCallback(GLFWwindow* w, int width, int height);
    static void maximizeCallback(GLFWwindow* w, int maximized);
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double x, double y);
    static void scrollCallback(GLFWwindow* w, double dx, double dy);

    void onPos(int x, int y);
    void onSize(int width, int height);
    void onMaximize(int maximized);

    void syncFromWindow();
    GLFWmonitor* currentMonitor() const;
    GLFWmonitor* monitorByIndex(int index) const;
    int monitorIndex(GLFWmonitor* monitor) const;
    void applyWindowed();
    void applyFullscreen(GLFWmonitor* monitor);

    Ref<Input::Keyboard> keyboard_;
    Ref<Input::Mouse> mouse_;
    GLFWwindow* window_ = nullptr;
    State state_{};
};