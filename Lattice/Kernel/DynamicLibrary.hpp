#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Lattice/Engine/PluginHost.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/*
Кроссплатформенная runtime-обертка над динамической библиотекой.

Класс владеет системным handle, полученным через dlopen/LoadLibrary, умеет
разрешать экспортируемые символы и хранит базовую информацию о загруженном
плагине. Он нужен для того, чтобы изолировать платформенно-зависимую логику
загрузки библиотек от остального движка и дать PluginLoader простой RAII-интерфейс.
DynamicLibrary не решает, какие плагины нужно искать и как они регистрируются
в движке: это ответственность PluginLoader и функции plugin_init.
*/

class DynamicLibrary {
public:
    DynamicLibrary() = default;
    explicit DynamicLibrary(const std::filesystem::path& path) { open(path); }

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    DynamicLibrary(DynamicLibrary&& other) noexcept { *this = std::move(other); }

    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        close();
        handle = other.handle;
        path = std::move(other.path);
        info = other.info;
        lastError_ = std::move(other.lastError_);
        other.handle = nullptr;
        other.path.clear();
        other.info = {};
        other.lastError_.clear();
        return *this;
    }

    ~DynamicLibrary() { close(); }

    bool open(const std::filesystem::path& path) {
        close();
        this->path.clear();
        info = {};
        lastError_.clear();

#ifdef _WIN32
        handle = ::LoadLibraryW(path.c_str());
        if (handle == nullptr) {
            lastError_ = formatWindowsError(::GetLastError());
            return false;
        }
#else
        handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            const char* error = ::dlerror();
            lastError_ = error != nullptr ? error : "dlopen failed";
            return false;
        }
#endif

        this->path = path;
        return true;
    }

    void close() {
        if (handle == nullptr) {
            return;
        }

#ifdef _WIN32
        ::FreeLibrary(handle);
#else
        ::dlclose(handle);
#endif
        handle = nullptr;
        path.clear();
        info = {};
    }

    [[nodiscard]] bool isOpen() const noexcept { return handle != nullptr; }
    [[nodiscard]] const std::string& lastError() const noexcept { return lastError_; }

    static constexpr std::string_view extension() noexcept {
#ifdef _WIN32
        return ".dll";
#elif defined(__APPLE__)
        return ".dylib";
#else
        return ".so";
#endif
    }

    template<typename T>
    T symbol(std::string_view name) {
        static_assert(std::is_pointer_v<T>, "DynamicLibrary::symbol<T> expects a pointer type");

        if (handle == nullptr) {
            lastError_ = "library is not open";
            return nullptr;
        }

#ifdef _WIN32
        FARPROC proc = ::GetProcAddress(handle, std::string(name).c_str());
        if (proc == nullptr) {
            lastError_ = formatWindowsError(::GetLastError());
            return nullptr;
        }
        lastError_.clear();
        return reinterpret_cast<T>(proc);
#else
        ::dlerror();
        void* proc = ::dlsym(handle, std::string(name).c_str());
        const char* error = ::dlerror();
        if (error != nullptr) {
            lastError_ = error;
            return nullptr;
        }
        lastError_.clear();
        return reinterpret_cast<T>(proc);
#endif
    }

    std::filesystem::path path;
    PluginInfo info;

private:
    static std::string formatWindowsError(unsigned long errorCode) {
#ifdef _WIN32
        if (errorCode == 0) {
            return {};
        }

        LPSTR messageBuffer = nullptr;
        const DWORD size = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(errorCode),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&messageBuffer),
            0,
            nullptr
        );

        std::string message;
        if (size > 0 && messageBuffer != nullptr) {
            message.assign(messageBuffer, size);
            while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' ')) {
                message.pop_back();
            }
        } else {
            message = "Win32 error " + std::to_string(errorCode);
        }

        if (messageBuffer != nullptr) {
            ::LocalFree(messageBuffer);
        }

        return message;
#else
        (void)errorCode;
        return {};
#endif
    }

#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
    std::string lastError_{};
};
