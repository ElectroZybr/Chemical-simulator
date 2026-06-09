#pragma once

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
#endif

#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#ifndef _WIN32
#include <filesystem>
#include <limits.h>
#include <unistd.h>

#include "stb/stb_image.h"
#endif

#include <iostream>

#include "App/UserSettings.h"
#include "generated/AppVersion.h"

#ifndef _WIN32
inline std::filesystem::path executableDirectory() {
    char buffer[PATH_MAX + 1]{};
    const ssize_t len = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (len <= 0) {
        return std::filesystem::current_path();
    }
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
}

inline void setLinuxWindowIcon(GLFWwindow* window) {
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

inline GLFWwindow* createWindow(const UserSettings::WindowState& initialWindowState) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
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
    int monitorX = 0;
    int monitorY = 0;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);
    const int width = initialWindowState.fullscreen ? mode->width : initialWindowState.width;
    const int height = initialWindowState.fullscreen ? mode->height : initialWindowState.height;
    GLFWwindow* window =
        glfwCreateWindow(width, height, "LatticeLab " LATTICELAB_VERSION_STRING, initialWindowState.fullscreen ? monitor : nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }
    if (!initialWindowState.fullscreen) {
        glfwSetWindowPos(window, initialWindowState.x, initialWindowState.y);
        if (initialWindowState.maximized) {
            glfwMaximizeWindow(window);
        }
    }

#ifdef _WIN32
    if (HWND hwnd = glfwGetWin32Window(window)) {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        if (HICON bigIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR))) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
        }
        if (HICON smallIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR))) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        }
    }
#else
    setLinuxWindowIcon(window);
#endif

    return window;
}
