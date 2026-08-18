#pragma once

#include <chrono>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

#include <Lattice/Tools/LogStyle.hpp>

class Logger {
public:
    enum class ConsoleMode {
        Quiet,
        Default,
        Verbose,
        Trace,
    };

    enum class Level {
        Action,
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Fatal,
        Ok,
    };

    static void setConsoleMode(ConsoleMode mode) noexcept;
    static ConsoleMode consoleMode() noexcept;

    template <typename... TArgs>
    static void action(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Action, tag, std::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void trace(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Trace, tag, std::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void debug(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Debug, tag, std::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void info(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Info, tag, std::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void warning(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Warning, tag, std::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void error(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Error, tag, std::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void fatal(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Fatal, tag, std::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void ok(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Ok, tag, std::format(format, std::forward<TArgs>(args)...));
    }

private:
    static void print(Level level, std::string_view tag, const std::string& message);
    static std::mutex& mutex();
};

class LogScope {
public:
    template <typename... TArgs>
    LogScope(std::string_view tag,
             std::format_string<TArgs...> startFormat,
             TArgs&&... args)
        : tag_(tag)
        , finishMessage_("Initialized")
        , startTime_(Clock::now())
        , active_(true)
    {
        Logger::action(tag_, startFormat, std::forward<TArgs>(args)...);
    }

    template <typename... TArgs>
    LogScope(std::string_view tag,
             std::string_view finishMessage,
             std::format_string<TArgs...> startFormat,
             TArgs&&... args)
        : tag_(tag)
        , finishMessage_(finishMessage)
        , startTime_(Clock::now())
        , active_(true)
    {
        Logger::action(tag_, startFormat, std::forward<TArgs>(args)...);
    }

    template <typename... TArgs>
    void step(std::format_string<TArgs...> format, TArgs&&... args) const {
        Logger::info(tag_, format, std::forward<TArgs>(args)...);
    }

    void finish() noexcept {
        if (!active_) return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - startTime_).count();
        Logger::ok(tag_, "{} ({} us)", finishMessage_, elapsed);
        active_ = false;
    }

    template <typename... TArgs>
    void finish(std::format_string<TArgs...> format, TArgs&&... args) {
        if (!active_) return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - startTime_).count();
        Logger::ok(tag_, "{} ({} us)", std::format(format, std::forward<TArgs>(args)...), elapsed);
        active_ = false;
    }

    void cancel() noexcept { active_ = false; }

    ~LogScope() {
        if (active_) {
            finish();
        }
    }


private:
    using Clock = std::chrono::steady_clock;
    std::string tag_;
    std::string finishMessage_;
    Clock::time_point startTime_;
    bool active_;
};
