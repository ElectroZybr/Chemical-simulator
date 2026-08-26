#include "glfwWindow.hpp"

#include <algorithm>
#include <dlfcn.h>
#include <cstdlib>
#include <fstream>
#include <format>
#include <string>
#include <utility>
#include <filesystem>
#include <unistd.h>

#include <vector>

#ifdef _WIN32
    #include <windows.h>
    #ifdef NEAR
        #undef NEAR
    #endif
    #ifdef FAR
        #undef FAR
    #endif
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
    #include "MetalLayer.hpp"
#else
    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_WAYLAND
    #define STB_IMAGE_IMPLEMENTATION
    #if defined(__GNUC__) && !defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstringop-overflow"
    #endif
    #include "stb/stb_image.h"
    #if defined(__GNUC__) && !defined(__clang__)
        #pragma GCC diagnostic pop
    #endif
#endif
    #include <GLFW/glfw3native.h>

#include "Lattice/Kernel/Exception.hpp"
#include <Lattice/Tools/Logger.hpp>
#include "glfwKeyboard.hpp"
#include "glfwMouse.hpp"

namespace {

#ifdef __linux__
constexpr std::string_view kAppId = "LatticeLab";

std::filesystem::path executableDirectory() {
    char buffer[PATH_MAX + 1]{};
    ssize_t len = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (len <= 0) return std::filesystem::current_path();
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path moduleDirectory() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&executableDirectory), &info) && info.dli_fname)
        return std::filesystem::path(info.dli_fname).parent_path();
    return {};
}

std::filesystem::path findIconPath() {
    std::vector<std::filesystem::path> roots;
    auto addRoot = [&](std::filesystem::path p) {
        if (p.empty()) return;
        std::error_code ec;
        auto n = std::filesystem::weakly_canonical(p, ec);
        if (!ec) p = std::move(n);
        if (std::find(roots.begin(), roots.end(), p) == roots.end())
            roots.push_back(std::move(p));
    };
    addRoot(std::filesystem::current_path());
    addRoot(executableDirectory());
    addRoot(moduleDirectory());

    for (auto root : roots) {
        for (int i = 0; i < 6 && !root.empty(); ++i) {
            const auto path = root / "assets" / "icon.png";
            std::error_code ec;
            if (std::filesystem::is_regular_file(path, ec))
                return path;
            if (!root.has_parent_path() || root == root.parent_path())
                break;
            root = root.parent_path();
        }
    }
    return {};
}

std::filesystem::path xdgDataHome() {
    if (const char* d = std::getenv("XDG_DATA_HOME"); d && *d)
        return d;
    if (const char* h = std::getenv("HOME"); h && *h)
        return std::filesystem::path(h) / ".local" / "share";
    return {};
}

bool installDesktopIcon(const std::filesystem::path& iconPng) {
    const auto data = xdgDataHome();
    if (data.empty()) {
        Logger::warning("Window", "cannot install desktop icon: no HOME/XDG_DATA_HOME");
        return false;
    }

    std::error_code ec;
    const auto iconDir = data / "icons" / "hicolor" / "256x256" / "apps";
    const auto appDir = data / "applications";
    std::filesystem::create_directories(iconDir, ec);
    std::filesystem::create_directories(appDir, ec);

    const auto iconDst = iconDir / std::format("{}.png", kAppId);
    std::filesystem::copy_file(iconPng, iconDst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        Logger::warning("Window", "failed to copy icon to {}: {}", iconDst.string(), ec.message());
        return false;
    }

    const auto desktop = appDir / std::format("{}.desktop", kAppId);
    const auto exe = executableDirectory() / "LatticeLab";
    std::ofstream out(desktop);
    if (!out) {
        Logger::warning("Window", "failed to write {}", desktop.string());
        return false;
    }
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=LatticeLab\n"
        << "Exec=" << exe.string() << "\n"
        << "Icon=" << iconDst.string() << "\n"
        << "StartupWMClass=" << kAppId << "\n"
        << "Terminal=false\n"
        << "Categories=Science;Education;\n";
    out.close();
    Logger::info("Window", "Wayland app icon via {} (Icon={})", desktop.string(), iconDst.string());
    return true;
}

bool applyX11Icon(GLFWwindow* window, const std::filesystem::path& iconPng) {
    int w = 0, h = 0, c = 0;
    unsigned char* pixels = stbi_load(iconPng.string().c_str(), &w, &h, &c, 4);
    if (!pixels) {
        Logger::warning("Window", "icon decode failed: {} ({})",
            iconPng.string(), stbi_failure_reason() ? stbi_failure_reason() : "unknown");
        return false;
    }
    GLFWimage icon{w, h, pixels};
    glfwSetWindowIcon(window, 1, &icon);
    stbi_image_free(pixels);
    Logger::info("Window", "X11 icon {}x{} from {}", w, h, iconPng.string());
    return true;
}
#endif // __linux__

bool monitorWorkArea(GLFWmonitor* monitor, int& x, int& y, int& w, int& h) {
    if (!monitor) return false;
    glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
    return w > 0 && h > 0;
}

} // namespace

// ============================================================
// ctor / dtor / move
// ============================================================

glfwWindow::glfwWindow(Lattice::Components& components) 
    : keyboard_(components.add<InputAPI, Input::Keyboard>())
    , mouse_(components.add<InputAPI, Input::Mouse>()) {
    if (!glfwInit()) {
        throw Lattice::Exception(tag, "Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#if defined(GLFW_X11_CLASS_NAME)
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "LatticeLab");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "LatticeLab");
#endif
#if defined(GLFW_WAYLAND_APP_ID)
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "LatticeLab");
#endif
#ifdef __linux__
    const auto iconPath = findIconPath();
    if (iconPath.empty()) {
        Logger::warning("Window", "icon not found (assets/icon.png from cwd/exe/plugin)");
    } else if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        installDesktopIcon(iconPath);
    }
#endif

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitors && state_.monitorIndex >= 0 && state_.monitorIndex < monitorCount) {
        monitor = monitors[state_.monitorIndex];
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    const int w = state_.fullscreen && mode ? mode->width  : state_.width;
    const int h = state_.fullscreen && mode ? mode->height : state_.height;

    const std::string title = std::format("{} {}", state_.name, BUILD_VERSION);

    window_ = glfwCreateWindow(
        w, h,
        title.c_str(),
        state_.fullscreen ? monitor : nullptr,
        nullptr);

    if (!window_) {
        glfwTerminate();
        throw Lattice::Exception(tag, "Failed to create GLFW window");
    }

    if (!state_.fullscreen) {
        glfwSetWindowPos(window_, state_.x, state_.y);
        if (state_.maximized) {
            glfwMaximizeWindow(window_);
        }
    }

#ifdef _WIN32
    if (HWND hwnd = glfwGetWin32Window(window_)) {
        HINSTANCE inst = GetModuleHandleW(nullptr);
        if (HICON big = static_cast<HICON>(LoadImageW(
                inst, MAKEINTRESOURCEW(101), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR))) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
        }
        if (HICON small = static_cast<HICON>(LoadImageW(
                inst, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR))) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
        }
    }
    Logger::info("Window", "created {}x{} at {},{} (Win32)", w, h, state_.x, state_.y);
#else
    const int platform = glfwGetPlatform();
    const char* platformName =
        platform == GLFW_PLATFORM_WAYLAND ? "Wayland" :
        platform == GLFW_PLATFORM_X11 ? "X11" :
        platform == GLFW_PLATFORM_COCOA ? "Cocoa" : "unknown";
    Logger::info("Window", "created {}x{} at {},{} ({})", w, h, state_.x, state_.y, platformName);
#ifdef __linux__
    if (!iconPath.empty() && platform == GLFW_PLATFORM_X11)
        applyX11Icon(window_, iconPath);
#endif
#endif

    glfwSetWindowUserPointer(window_, this);
    glfwSetWindowPosCallback(window_, posCallback);
    glfwSetWindowSizeCallback(window_, sizeCallback);
    glfwSetWindowMaximizeCallback(window_, maximizeCallback);

    glfwSetKeyCallback(window_, keyCallback);

    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);

    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window_, &x, &y);
    mouse_.setPosition(static_cast<float>(x), static_cast<float>(y));
    mouse_.resetDelta();

    if (!state_.fullscreen) {
        syncFromWindow();
    }

    show();
}

glfwWindow::~glfwWindow() {
    if (window_) {
        glfwSetWindowUserPointer(window_, nullptr);
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
}

// glfwWindow::glfwWindow(glfwWindow&& other) noexcept
//     : window_(std::exchange(other.window_, nullptr))
//     , state_(other.state_)
// {
//     if (window_) {
//         glfwSetWindowUserPointer(window_, this);
//     }
// }

glfwWindow& glfwWindow::operator=(glfwWindow&& other) noexcept {
    if (this != &other) {
        if (window_) {
            glfwSetWindowUserPointer(window_, nullptr);
            glfwDestroyWindow(window_);
        }
        window_ = std::exchange(other.window_, nullptr);
        state_  = other.state_;
        if (window_) {
            glfwSetWindowUserPointer(window_, this);
        }
    }
    return *this;
}

// ============================================================
// WindowAPI
// ============================================================

bool glfwWindow::shouldClose() const {
    return window_ && glfwWindowShouldClose(window_);
}

void glfwWindow::requestClose() {
    if (window_) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

void glfwWindow::pollEvents() {
    keyboard_.beginFrame();
    mouse_.beginFrame();
    glfwPollEvents();
}

glm::vec2 glfwWindow::windowSize() const {
    if (!window_) return {0.f, 0.f};
    int w = 0, h = 0;
    glfwGetWindowSize(window_, &w, &h);
    return {static_cast<float>(w), static_cast<float>(h)};
}

glm::vec2 glfwWindow::framebufferSize() const {
    if (!window_) return {0.f, 0.f};
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    return {static_cast<float>(w), static_cast<float>(h)};
}

float glfwWindow::contentScale() const {
    if (!window_) return 1.f;
    float x = 1.f, y = 1.f;
    glfwGetWindowContentScale(window_, &x, &y);
    return x;
}

bool glfwWindow::fullscreen() const {
    return state_.fullscreen;
}

void glfwWindow::setFullscreen(bool enabled) {
    if (!window_ || enabled == state_.fullscreen) return;

    if (enabled) {
        syncFromWindow();
        auto* monitor = currentMonitor();
        if (!monitor) monitor = monitorByIndex(state_.monitorIndex);
        if (!monitor) return;

        state_.monitorIndex = monitorIndex(monitor);
        applyFullscreen(monitor);
        state_.fullscreen = true;
        Logger::info("Window", "fullscreen on (monitor {})", state_.monitorIndex);
    } else {
        applyWindowed();
        state_.fullscreen = false;
        syncFromWindow();
        Logger::info("Window", "fullscreen off {}x{} at {},{}",
            state_.width, state_.height, state_.x, state_.y);
    }
}

void glfwWindow::show() {
    if (window_) {
        glfwShowWindow(window_);
    }
}

void glfwWindow::setTitle(std::string_view title) {
    if (window_) {
        glfwSetWindowTitle(window_, std::string(title).c_str());
    }
}

NativeWindow glfwWindow::native() const {
    NativeWindow n{};
    if (!window_) return n;

#if defined(_WIN32)
    n.kind = NativeWindow::Kind::Win32;
    n.window = glfwGetWin32Window(window_);

#elif defined(__APPLE__)
    n.kind = NativeWindow::Kind::Metal;
    n.window = glfwGetCocoaWindow(window_);
    n.extra = glfwMetalLayer(window_);

#else
    const int platform = glfwGetPlatform(); // GLFW ≥ 3.4

    if (platform == GLFW_PLATFORM_WAYLAND) {
        n.kind = NativeWindow::Kind::Wayland;
        n.display = glfwGetWaylandDisplay();
        n.window  = glfwGetWaylandWindow(window_); // wl_surface*
    } else if (platform == GLFW_PLATFORM_X11) {
        n.kind = NativeWindow::Kind::X11;
        n.display = glfwGetX11Display();
        n.window  = reinterpret_cast<void*>(
            static_cast<uintptr_t>(glfwGetX11Window(window_)));
    } else {
        Logger::error("Window", "unsupported glfw platform {}", platform);
    }
#endif
    return n;
}

// ============================================================
// Callbacks
// ============================================================

void glfwWindow::posCallback(GLFWwindow* w, int x, int y) {
    if (auto* self = static_cast<glfwWindow*>(glfwGetWindowUserPointer(w)))
        self->onPos(x, y);
}

void glfwWindow::sizeCallback(GLFWwindow* w, int width, int height) {
    if (auto* self = static_cast<glfwWindow*>(glfwGetWindowUserPointer(w)))
        self->onSize(width, height);
}

void glfwWindow::maximizeCallback(GLFWwindow* w, int maximized) {
    if (auto* self = static_cast<glfwWindow*>(glfwGetWindowUserPointer(w)))
        self->onMaximize(maximized);
}

void glfwWindow::keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (auto* self = static_cast<glfwWindow*>(glfwGetWindowUserPointer(w))) {
        self->keyboard_.onKey(Input::keyFromGlfw(key), Input::keyActionFromGlfw(action));
    }
}

void glfwWindow::mouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
    auto* self = static_cast<glfwWindow*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    const auto btn = Input::mouseButtonFromGlfw(button);
    if (btn == Input::MouseButton::Count) return;
    self->mouse_.onButton(btn, Input::buttonActionFromGlfw(action));
    Logger::debug("Window", "click");
}

void glfwWindow::cursorPosCallback(GLFWwindow* w, double x, double y) {
    if (auto* self = static_cast<glfwWindow*>(glfwGetWindowUserPointer(w)))
        self->mouse_.onMove(static_cast<float>(x), static_cast<float>(y));
}

void glfwWindow::scrollCallback(GLFWwindow* w, double dx, double dy) {
    if (auto* self = static_cast<glfwWindow*>(glfwGetWindowUserPointer(w)))
        self->mouse_.onScroll(static_cast<float>(dx), static_cast<float>(dy));
}

void glfwWindow::onPos(int x, int y) {
    if (state_.fullscreen || state_.maximized) return;
    state_.x = x;
    state_.y = y;
    state_.monitorIndex = monitorIndex(currentMonitor());
    Logger::debug("Window", "moved {},{}", x, y);
}

void glfwWindow::onSize(int width, int height) {
    if (state_.fullscreen || state_.maximized || width <= 0 || height <= 0) return;
    state_.width = width;
    state_.height = height;
    state_.monitorIndex = monitorIndex(currentMonitor());
    Logger::info("Window", "resized {}x{}", width, height);
}

void glfwWindow::onMaximize(int maximized) {
    if (state_.fullscreen) return;
    state_.maximized = (maximized == GLFW_TRUE);
    Logger::info("Window", "{}", state_.maximized ? "maximized" : "restored");
    if (!state_.maximized) {
        syncFromWindow();
    } else {
        state_.monitorIndex = monitorIndex(currentMonitor());
    }
}

// ============================================================
// Helpers
// ============================================================

GLFWmonitor* glfwWindow::monitorByIndex(int index) const {
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (!monitors || index < 0 || index >= count) return nullptr;
    return monitors[index];
}

int glfwWindow::monitorIndex(GLFWmonitor* monitor) const {
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (!monitors || !monitor) return 0;
    for (int i = 0; i < count; ++i)
        if (monitors[i] == monitor) return i;
    return 0;
}

GLFWmonitor* glfwWindow::currentMonitor() const {
    if (!window_) return glfwGetPrimaryMonitor();

    if (auto* fs = glfwGetWindowMonitor(window_))
        return fs;

    int wx = 0, wy = 0, ww = 0, wh = 0;
    glfwGetWindowPos(window_, &wx, &wy);
    glfwGetWindowSize(window_, &ww, &wh);
    const int cx = wx + ww / 2;
    const int cy = wy + wh / 2;

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (!monitors || count <= 0) return glfwGetPrimaryMonitor();

    GLFWmonitor* best = monitors[0];
    int bestOverlap = -1;

    for (int i = 0; i < count; ++i) {
        int mx = 0, my = 0;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        if (!mode) continue;

        if (cx >= mx && cx < mx + mode->width &&
            cy >= my && cy < my + mode->height)
            return monitors[i];

        const int ow = std::max(0, std::min(wx + ww, mx + mode->width)  - std::max(wx, mx));
        const int oh = std::max(0, std::min(wy + wh, my + mode->height) - std::max(wy, my));
        const int overlap = ow * oh;
        if (overlap > bestOverlap) {
            bestOverlap = overlap;
            best = monitors[i];
        }
    }
    return best;
}

void glfwWindow::syncFromWindow() {
    if (!window_ || state_.fullscreen) return;

    state_.maximized = glfwGetWindowAttrib(window_, GLFW_MAXIMIZED) == GLFW_TRUE;
    auto* monitor = currentMonitor();
    state_.monitorIndex = monitorIndex(monitor);

    if (!state_.maximized) {
        glfwGetWindowPos(window_, &state_.x, &state_.y);
        glfwGetWindowSize(window_, &state_.width, &state_.height);
        return;
    }

    if (monitor && monitorWorkArea(monitor, state_.x, state_.y, state_.width, state_.height))
        return;

    glfwGetWindowPos(window_, &state_.x, &state_.y);
    glfwGetWindowSize(window_, &state_.width, &state_.height);
}

void glfwWindow::applyWindowed() {
    if (!window_) return;

    int x = state_.x, y = state_.y, w = state_.width, h = state_.height;
    if (state_.maximized) {
        if (auto* mon = monitorByIndex(state_.monitorIndex))
            monitorWorkArea(mon, x, y, w, h);
    }

    glfwSetWindowMonitor(window_, nullptr, x, y, w, h, GLFW_DONT_CARE);
    if (state_.maximized) glfwMaximizeWindow(window_);
    glfwFocusWindow(window_);
}

void glfwWindow::applyFullscreen(GLFWmonitor* monitor) {
    if (!window_ || !monitor) return;
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) return;

    glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    glfwFocusWindow(window_);
}
