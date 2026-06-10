#pragma once

#include <GLFW/glfw3.h>

#include "App/UserSettings.h"

class WindowController {
public:
    static void init(GLFWwindow* window, const UserSettings::WindowState& initialWindowState);
    static void toggleFullscreen();
    static void requestToggleFullscreen();
    static void processPendingToggle();
    static UserSettings::WindowState snapshot();

private:
    static void windowPosCallback(GLFWwindow* window, int x, int y);
    static void windowSizeCallback(GLFWwindow* window, int width, int height);
    static void windowMaximizeCallback(GLFWwindow* window, int maximized);
    static void syncWindowedStateFromWindow();
    static GLFWmonitor* currentMonitor();
    static GLFWmonitor* monitorByIndex(int index);
    static int monitorIndex(GLFWmonitor* monitor);
    static void clampWindowedStateToMonitor();
    static void applyWindowedState();
    static bool applyFullscreen(GLFWmonitor* monitor);
    static void processPendingWindowedMaximize();

    static GLFWwindow* window;
    static bool isFullscreen;
    static bool fullscreenToggleRequested;
    static bool windowedMaximizeRequested;
    static int windowedMaximizeDelayFrames;
    static bool windowedWasMaximized;
    static int windowedMonitorIndex;
    static int windowedX;
    static int windowedY;
    static int windowedWidth;
    static int windowedHeight;
};
