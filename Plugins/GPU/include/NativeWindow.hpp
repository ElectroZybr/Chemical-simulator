#pragma once

struct NativeWindow {
    enum class Kind { None, Win32, X11, Wayland, Cocoa, Web, Headless };
    Kind kind = Kind::None;
    void* display = nullptr;
    void* window  = nullptr;
    void* extra   = nullptr;
};