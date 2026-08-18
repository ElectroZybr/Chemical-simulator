#include "WindowController.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#ifdef NEAR
#undef NEAR
#endif
#ifdef FAR
#undef FAR
#endif
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#else
#include <limits.h>
#include <unistd.h>

#include "App/StbImage.h"
#endif

#include <Lattice/Tools/Logger.hpp>
#include "generated/AppVersion.h"

namespace {
#ifndef _WIN32
std::filesystem::path executableDirectory() {
    char buffer[PATH_MAX + 1]{};
    const ssize_t len = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (len <= 0) {
        return std::filesystem::current_path();
    }
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
}

void setLinuxWindowIcon(GLFWwindow* window) {
    const std::filesystem::path exeDir = executableDirectory();
    const std::filesystem::path candidates[] = {
        std::filesystem::path("assets/icon.png"),
        exeDir / "assets/icon.png",
    };

    for (const auto& path : candidates) {
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        if (!pixels) {
            continue;
        }

        GLFWimage icon{width, height, pixels};
        glfwSetWindowIcon(window, 1, &icon);
        stbi_image_free(pixels);
        return;
    }

    std::cerr << "Failed to load window icon: assets/icon.png" << std::endl;
}
#endif

void logWindowState(const char* label, GLFWwindow* window, bool isFullscreen, bool windowedWasMaximized) {
    if (!window) {
        Logger::warning("WindowController", "{} window={}", label, "null");
        return;
    }

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    glfwGetWindowPos(window, &x, &y);
    glfwGetWindowSize(window, &width, &height);

    Logger::trace(
        "WindowController",
        "{} ptr={} fullscreen={} monitor={} decorated={} maximized={} focused={} iconified={} savedMaximized={} pos={} size={}",
        label,
        static_cast<const void*>(window),
        isFullscreen,
        static_cast<const void*>(glfwGetWindowMonitor(window)),
        glfwGetWindowAttrib(window, GLFW_DECORATED),
        glfwGetWindowAttrib(window, GLFW_MAXIMIZED),
        glfwGetWindowAttrib(window, GLFW_FOCUSED),
        glfwGetWindowAttrib(window, GLFW_ICONIFIED),
        windowedWasMaximized,
        std::format("({},{})", x, y),
        std::format("({}x{})", width, height));
}

bool monitorWorkArea(GLFWmonitor* monitor, int& x, int& y, int& width, int& height) {
    if (!monitor) {
        return false;
    }

    glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
    return width > 0 && height > 0;
}
} // namespace

GLFWwindow* WindowController::window = nullptr;
bool WindowController::isFullscreen = false;
bool WindowController::windowedWasMaximized = false;
int WindowController::windowedMonitorIndex = 0;
int WindowController::windowedX = 160;
int WindowController::windowedY = 120;
int WindowController::windowedWidth = 1280;
int WindowController::windowedHeight = 720;

GLFWmonitor* WindowController::monitorByIndex(int index) {
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (!monitors || index < 0 || index >= monitorCount) {
        return nullptr;
    }
    return monitors[index];
}

int WindowController::monitorIndex(GLFWmonitor* monitor) {
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (!monitors || !monitor) {
        return 0;
    }

    for (int i = 0; i < monitorCount; ++i) {
        if (monitors[i] == monitor) {
            return i;
        }
    }
    return 0;
}

GLFWmonitor* WindowController::currentMonitor() {
    if (!window) {
        return glfwGetPrimaryMonitor();
    }

    if (GLFWmonitor* fullscreenMonitor = glfwGetWindowMonitor(window)) {
        return fullscreenMonitor;
    }

    int windowX = 0;
    int windowY = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowPos(window, &windowX, &windowY);
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    const int centerX = windowX + windowWidth / 2;
    const int centerY = windowY + windowHeight / 2;

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (!monitors || monitorCount <= 0) {
        return glfwGetPrimaryMonitor();
    }

    GLFWmonitor* bestMonitor = monitors[0];
    int bestOverlap = -1;

    for (int i = 0; i < monitorCount; ++i) {
        int monitorX = 0;
        int monitorY = 0;
        glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        if (!mode) {
            continue;
        }

        if (centerX >= monitorX && centerX < monitorX + mode->width &&
            centerY >= monitorY && centerY < monitorY + mode->height) {
            return monitors[i];
        }

        const int overlapWidth =
            std::max(0, std::min(windowX + windowWidth, monitorX + mode->width) - std::max(windowX, monitorX));
        const int overlapHeight =
            std::max(0, std::min(windowY + windowHeight, monitorY + mode->height) - std::max(windowY, monitorY));
        const int overlap = overlapWidth * overlapHeight;

        if (overlap > bestOverlap) {
            bestOverlap = overlap;
            bestMonitor = monitors[i];
        }
    }

    return bestMonitor;
}

void WindowController::windowPosCallback(GLFWwindow* callbackWindow, int x, int y) {
    if (callbackWindow != window || isFullscreen || windowedWasMaximized) {
        return;
    }

    windowedX = x;
    windowedY = y;
    windowedMonitorIndex = monitorIndex(currentMonitor());
}

void WindowController::windowSizeCallback(GLFWwindow* callbackWindow, int width, int height) {
    if (callbackWindow != window || isFullscreen || windowedWasMaximized || width <= 0 || height <= 0) {
        return;
    }

    windowedWidth = width;
    windowedHeight = height;
    windowedMonitorIndex = monitorIndex(currentMonitor());
}

void WindowController::windowMaximizeCallback(GLFWwindow* callbackWindow, int maximized) {
    if (callbackWindow != window || isFullscreen) {
        return;
    }

    windowedWasMaximized = maximized == GLFW_TRUE;
    if (!windowedWasMaximized) {
        syncWindowedStateFromWindow();
    } else {
        windowedMonitorIndex = monitorIndex(currentMonitor());
    }
}

void WindowController::syncWindowedStateFromWindow() {
    if (!window || isFullscreen) {
        return;
    }

    windowedWasMaximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
    GLFWmonitor* monitor = currentMonitor();
    windowedMonitorIndex = monitorIndex(monitor);
    if (!windowedWasMaximized) {
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
        return;
    }

    if (monitor && monitorWorkArea(monitor, windowedX, windowedY, windowedWidth, windowedHeight)) {
        return;
    }

    glfwGetWindowPos(window, &windowedX, &windowedY);
    glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
}

void WindowController::applyWindowedState() {
    if (!window) {
        return;
    }

    int targetX = windowedX;
    int targetY = windowedY;
    int targetWidth = windowedWidth;
    int targetHeight = windowedHeight;
    if (windowedWasMaximized) {
        if (GLFWmonitor* monitor = monitorByIndex(windowedMonitorIndex)) {
            monitorWorkArea(monitor, targetX, targetY, targetWidth, targetHeight);
        }
    }

    glfwSetWindowMonitor(window, nullptr, targetX, targetY, targetWidth, targetHeight, GLFW_DONT_CARE);
    if (windowedWasMaximized) {
        glfwMaximizeWindow(window);
    }
    glfwFocusWindow(window);
}

void WindowController::applyFullscreen(GLFWmonitor* monitor) {
    if (!window || !monitor) {
        return;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        return;
    }

    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    glfwFocusWindow(window);
}

void WindowController::init(GLFWwindow* window, const UserSettings::WindowState& initialWindowState) {
    WindowController::window = window;
    isFullscreen = initialWindowState.fullscreen;
    windowedWasMaximized = initialWindowState.maximized;
    windowedMonitorIndex = initialWindowState.monitorIndex;
    windowedX = initialWindowState.x;
    windowedY = initialWindowState.y;
    windowedWidth = initialWindowState.width;
    windowedHeight = initialWindowState.height;

    glfwSetWindowPosCallback(window, windowPosCallback);
    glfwSetWindowSizeCallback(window, windowSizeCallback);
    glfwSetWindowMaximizeCallback(window, windowMaximizeCallback);

    if (!isFullscreen) {
        syncWindowedStateFromWindow();
    }
}

GLFWwindow* WindowController::create(const UserSettings::WindowState& initialWindowState) {
    LogScope scope("Window", "Creating main window", "Window created");
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        scope.cancel();
        return nullptr;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitors && initialWindowState.monitorIndex >= 0 && initialWindowState.monitorIndex < monitorCount) {
        monitor = monitors[initialWindowState.monitorIndex];
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    const int width = initialWindowState.fullscreen && mode ? mode->width : initialWindowState.width;
    const int height = initialWindowState.fullscreen && mode ? mode->height : initialWindowState.height;
    GLFWwindow* createdWindow =
        glfwCreateWindow(width, height, "LatticeLab " LATTICELAB_VERSION_STRING, initialWindowState.fullscreen ? monitor : nullptr, nullptr);
    if (!createdWindow) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        scope.cancel();
        return nullptr;
    }

    if (!initialWindowState.fullscreen) {
        glfwSetWindowPos(createdWindow, initialWindowState.x, initialWindowState.y);
        if (initialWindowState.maximized) {
            glfwMaximizeWindow(createdWindow);
        }
    }

#ifdef _WIN32
    if (HWND hwnd = glfwGetWin32Window(createdWindow)) {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        if (HICON bigIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR))) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
        }
        if (HICON smallIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR))) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        }
    }
#else
    setLinuxWindowIcon(createdWindow);
#endif

    init(createdWindow, initialWindowState);
    return createdWindow;
}

std::pair<int, int> WindowController::framebufferSize() {
    if (!window) {
        return {0, 0};
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    return {width, height};
}

void WindowController::toggleFullscreen() {
    if (!window) {
        return;
    }

    logWindowState("before-toggle", window, isFullscreen, windowedWasMaximized);

    if (isFullscreen) {
        applyWindowedState();
        isFullscreen = false;
        syncWindowedStateFromWindow();
        logWindowState("after-toggle", window, isFullscreen, windowedWasMaximized);
        return;
    }

    syncWindowedStateFromWindow();
    GLFWmonitor* monitor = currentMonitor();
    if (!monitor) {
        monitor = monitorByIndex(windowedMonitorIndex);
    }
    if (!monitor) {
        return;
    }
    windowedMonitorIndex = monitorIndex(monitor);
    applyFullscreen(monitor);
    isFullscreen = true;
    logWindowState("after-toggle", window, isFullscreen, windowedWasMaximized);
}

UserSettings::WindowState WindowController::snapshot() {
    UserSettings::WindowState state{};
    state.fullscreen = isFullscreen;
    state.maximized = windowedWasMaximized;
    state.monitorIndex = isFullscreen ? windowedMonitorIndex : monitorIndex(currentMonitor());
    state.x = windowedX;
    state.y = windowedY;
    state.width = windowedWidth;
    state.height = windowedHeight;

    if (!state.fullscreen && window) {
        syncWindowedStateFromWindow();
        state.maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
        state.monitorIndex = monitorIndex(currentMonitor());
        if (state.maximized) {
            GLFWmonitor* monitor = monitorByIndex(state.monitorIndex);
            if (!monitorWorkArea(monitor, state.x, state.y, state.width, state.height)) {
                glfwGetWindowPos(window, &state.x, &state.y);
                glfwGetWindowSize(window, &state.width, &state.height);
            }
        }
        else {
            glfwGetWindowPos(window, &state.x, &state.y);
            glfwGetWindowSize(window, &state.width, &state.height);
        }
    }

    return state;
}
