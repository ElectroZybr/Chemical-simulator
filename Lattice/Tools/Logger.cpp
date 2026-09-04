#include <Lattice/Tools/Logger.hpp>
#include "Lattice/Tools/LogStyle.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

std::atomic<Logger::ConsoleMode> gConsoleMode = Logger::ConsoleMode::Default;

struct LevelStyle {
    std::string_view label;
    std::string_view status;
    Lattice::TextStyle style;
    Logger::ConsoleMode consoleMode;
    bool useStdErr;
};

std::string timestampForLogLine() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif

    std::ostringstream out;
    out << std::put_time(&localTime, "%H:%M:%S");
    return out.str();
}

std::ofstream& logFile() {
    static std::ofstream file = [] {
        const auto path = Logger::logPath();

        std::filesystem::create_directories(path.parent_path());

        return std::ofstream(
            path,
            std::ios::out | std::ios::trunc
        );
    }();

    return file;
}

LevelStyle levelStyle(Logger::Level level) {
    using Level = Logger::Level;

    switch (level) {
        case Level::Ok:
            return {"OK", "✓", TextStyle::Green, Logger::ConsoleMode::Default, false};
        case Level::Warning:
            return {"WARN", "⚠", TextStyle::Yellow, Logger::ConsoleMode::Default, true};
        case Level::Error:
            return {"ERROR", "⚠", TextStyle::Red, Logger::ConsoleMode::Default, true};
        case Level::Exception:
            return {"EXCEPTION", "✗", TextStyle::Red, Logger::ConsoleMode::Default, true};
        case Level::Action:
            return {"ACTION", "➜", TextStyle::Cyan, Logger::ConsoleMode::Default, false};
        case Level::Info:
            return {"INFO", "•", TextStyle::Gray, Logger::ConsoleMode::Default, false};
        case Level::Trace:
            return {"TRACE", "·", TextStyle::Gray, Logger::ConsoleMode::Trace, false};
    }

    return {"INFO", "•", TextStyle::None, Logger::ConsoleMode::Verbose, false};
}

}

void Logger::treeLine(std::string_view message) {
    std::lock_guard lock(mutex());

    std::ofstream& file = logFile();

    if (file.is_open()) {
        file << timestampForLogLine()
             << ' '
             << message
             << '\n';

        file.flush();
    }

    std::cout
        << Color::gray
        << message
        << Color::reset
        << '\n';
}

void Logger::setConsoleMode(ConsoleMode mode) noexcept {
    gConsoleMode.store(mode, std::memory_order_relaxed);
}

Logger::ConsoleMode Logger::consoleMode() noexcept {
    return gConsoleMode.load(std::memory_order_relaxed);
}

std::mutex& Logger::mutex() {
    static std::mutex value;
    return value;
}

size_t& Logger::indent() {
    thread_local size_t value = 0;
    return value;
}

std::vector<Logger::ScopeState>& Logger::scopes() {
    thread_local std::vector<ScopeState> value;
    return value;
}

void Logger::print(const Text& text, OutputMode mode) {
    std::lock_guard lock(mutex());

    std::ofstream& file = logFile();
    if (file.is_open()) {
        file << timestampForLogLine() << ' ' << text.plain() << '\n';
        file.flush();
    }

    if (mode == OutputMode::Persistent || consoleMode() != ConsoleMode::Default) {
        invalidateErase(mode);

        Text wrapped = text.wrap(100, indent());
        std::cout << wrapped.render() << '\n';

        if (mode == OutputMode::Transient)
            addScopeLines(wrapped.lines());
    }
}

void Logger::print(Level level, std::string_view tag, const Text& text, OutputMode mode) {
    std::lock_guard lock(mutex());

    const LevelStyle style = levelStyle(level);

    std::ofstream& file = logFile();
    if (file.is_open()) {
        file << timestampForLogLine() << ' '
             << std::format("[{}] [{}] {}", style.label, tag, text.plain())
             << '\n';
        file.flush();
    }

    if (static_cast<int>(consoleMode()) < static_cast<int>(style.consoleMode))
        return;

    invalidateErase(mode);

    Text line;
    if (indent() > 0)
        line.append(std::string(indent(), ' '));
    line.append(style.status, style.style);
    line.append(" [");
    line.append(tag, TextStyle::Bold);
    line.append("] ");
    line.append(text, style.style);

    std::cout << line.render() << '\n';

    if (mode == OutputMode::Transient)
        addScopeLines(line.lines());
}