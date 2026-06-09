#include "WindowController.h"

#include <algorithm>
#include <iostream>

namespace {
void logWindowState(const char* label, GLFWwindow* window, bool isFullscreen, bool windowedWasMaximized) {
    if (!window) {
        std::cerr << "[WindowController] " << label << " window=null" << std::endl;
        return;
    }

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    glfwGetWindowPos(window, &x, &y);
    glfwGetWindowSize(window, &width, &height);

    std::cerr
        << "[WindowController] " << label
        << " ptr=" << window
        << " fullscreen=" << isFullscreen
        << " monitor=" << glfwGetWindowMonitor(window)
        << " decorated=" << glfwGetWindowAttrib(window, GLFW_DECORATED)
        << " maximized=" << glfwGetWindowAttrib(window, GLFW_MAXIMIZED)
        << " focused=" << glfwGetWindowAttrib(window, GLFW_FOCUSED)
        << " iconified=" << glfwGetWindowAttrib(window, GLFW_ICONIFIED)
        << " savedMaximized=" << windowedWasMaximized
        << " pos=(" << x << "," << y << ")"
        << " size=(" << width << "x" << height << ")"
        << std::endl;
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
    windowedMonitorIndex = monitorIndex(currentMonitor());
    if (!windowedWasMaximized) {
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
    }
}

void WindowController::applyWindowedState() {
    if (!window) {
        return;
    }

    glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, GLFW_DONT_CARE);
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
    GLFWmonitor* monitor = monitorByIndex(windowedMonitorIndex);
    if (!monitor) {
        monitor = currentMonitor();
    }
    if (!monitor) {
        return;
    }
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
        glfwGetWindowPos(window, &state.x, &state.y);
        glfwGetWindowSize(window, &state.width, &state.height);
        state.monitorIndex = monitorIndex(currentMonitor());
    }

    return state;
}
