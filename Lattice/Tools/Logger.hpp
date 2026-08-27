#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <Lattice/Tools/LogStyle.hpp>
#include <Lattice/Tools/Text.hpp>

class Logger {
public:
    class Indent {
    public:
        Indent() { ++Logger::indent(); }
        ~Indent() { --Logger::indent(); }
    };

    enum class OutputMode {
        Transient,
        Persistent
    };

    enum class ConsoleMode {
        Default,
        Verbose,
        Trace
    };

    enum class Level {
        Action,
        Trace,
        Info,
        Warning,
        Error,
        Exception,
        Ok
    };

    static std::filesystem::path logPath() {
        return std::filesystem::path("Logs") / "lattice.log";
    }

    static void setConsoleMode(ConsoleMode mode) noexcept;
    static ConsoleMode consoleMode() noexcept;

    template <typename... TArgs>
    static void message(std::format_string<TArgs...> format, TArgs&&... args) {
        print(Text::format(format, std::forward<TArgs>(args)...), OutputMode::Persistent);
    }

    template <typename... TArgs>
    static void action(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Action, tag, Text::format(format, std::forward<TArgs>(args)...), OutputMode::Transient);
    }

    template <typename... TArgs>
    static void trace(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Trace, tag, Text::format(format, std::forward<TArgs>(args)...), OutputMode::Transient);
    }

    template <typename... TArgs>
    static void info(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Info, tag, Text::format(format, std::forward<TArgs>(args)...), OutputMode::Transient);
    }

    template <typename... TArgs>
    static void warning(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Warning, tag, Text::format(format, std::forward<TArgs>(args)...), OutputMode::Persistent);
    }

    template <typename... TArgs>
    static void error(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Error, tag, Text::format(format, std::forward<TArgs>(args)...), OutputMode::Persistent);
    }

    template <typename... TArgs>
    static void exception(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Exception, tag, Text::format(format, std::forward<TArgs>(args)...), OutputMode::Persistent);
    }

    template <typename... TArgs>
    static void ok(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Ok, tag, Text::format(format, std::forward<TArgs>(args)...), OutputMode::Persistent);
    }

    static void ok(std::string_view tag, const Text& text) {
        print(Level::Ok, tag, text, OutputMode::Persistent);
    }

    static void exception(std::string_view tag, const Text& text) {
        print(Level::Exception, tag, text, OutputMode::Persistent);
    }

    static void treeLine(std::string_view message);

    template <typename... TArgs>
    static void tree(std::format_string<TArgs...> format, TArgs&&... args) {
        treeLine(std::format(format, std::forward<TArgs>(args)...));
    }

    static size_t& indent();
private:
    struct ScopeState {
        size_t transientLines = 0;
    };

    // static bool& hasOutput() {
    //     thread_local bool value = false;
    //     return value;
    // }

    static std::vector<ScopeState>& scopes();
    static std::mutex& mutex();

    static void print(const Text& text, OutputMode mode);
    static void print(Level level, std::string_view tag, const Text& text, OutputMode mode);

    static void addScopeLines(size_t count) {
        if (!scopes().empty())
            scopes().back().transientLines += count;
    }

    static void eraseLines(size_t count) {
        if (count == 0)
            return;
        for (size_t i = 0; i < count; ++i)
            std::cout << "\033[1A\033[2K";
        std::cout << '\r' << std::flush;
    }

    static bool& pendingGap() {
        thread_local bool value = false;
        return value;
    }

public:
    class Tree {
    public:
        class Node {
        public:
            explicit Node(std::string name)
                : name_(std::move(name)) {}

            Node& branch(std::string_view name) {
                children_.push_back(std::make_unique<Node>(std::string(name)));
                return *children_.back();
            }

            void node(std::string_view name) {
                children_.push_back(std::make_unique<Node>(std::string(name)));
            }

        private:
            friend class Tree;

            std::string name_;
            std::vector<std::unique_ptr<Node>> children_;
        };

        explicit Tree(std::string_view name)
            : root_(std::string(name)) {}

        Node& branch(std::string_view name) {
            return root_.branch(name);
        }

        void node(std::string_view name) {
            root_.node(name);
        }

        void node(std::string_view name, size_t depth) {
            while (parents_.size() > depth)
                parents_.pop_back();

            Node* parent = parents_.empty()
                ? &root_
                : parents_.back();

            Node& node = parent->branch(name);
            parents_.push_back(&node);
        }

        void print() const {
            Logger::treeLine(std::format("{}{}", Color::brightWhite, root_.name_));
            printNode(root_, "");
        }

    private:
        static void printNode(const Node& node, const std::string& prefix) {
            for (size_t i = 0; i < node.children_.size(); ++i) {
                const auto& child = node.children_[i];
                const bool last = i + 1 == node.children_.size();

                Logger::tree(
                    "{}{}─ {}{}",
                    prefix,
                    last ? "└" : "├",
                    Color::brightWhite,
                    child->name_
                );

                printNode(
                    *child,
                    prefix + (last ? "   " : "│  ")
                );
            }
        }

        Node root_;
        std::vector<Node*> parents_;
    };

    class Scope {
    public:
        template <typename... TArgs>
        Scope(std::string_view tag, std::format_string<TArgs...> startFormat, TArgs&&... args)
            : tag_(tag)
            , finishMessage_("Initialized")
            , startTime_(Clock::now())
            , active_(true)
        {
            Logger::scopes().push_back({});
            if (Logger::consoleMode() == ConsoleMode::Default && pendingGap()) {
                std::cout << '\n';
                addScopeLines(1);      // если тест пройдёт — этот \n сотрётся вместе с ➜/•
            }
            pendingGap() = false;
            Logger::action(tag_, startFormat, std::forward<TArgs>(args)...);
            ++Logger::indent();
        }

        template <typename... TArgs>
        Scope(std::string_view tag, std::string_view finishMessage,
              std::format_string<TArgs...> startFormat, TArgs&&... args)
            : tag_(tag)
            , finishMessage_(finishMessage)
            , startTime_(Clock::now())
            , active_(true)
        {
            Logger::scopes().push_back({});
            if (Logger::consoleMode() == ConsoleMode::Default && pendingGap()) {
                std::cout << '\n';
                addScopeLines(1);      // если тест пройдёт — этот \n сотрётся вместе с ➜/•
            }
            pendingGap() = false;
            Logger::action(tag_, startFormat, std::forward<TArgs>(args)...);
            ++Logger::indent();
        }

        void finish() noexcept {
            finish("{}", finishMessage_);
        }

        template <typename... TArgs>
        void finish(std::format_string<TArgs...> format, TArgs&&... args) {
            if (!active_)
                return;

            close(
                Logger::Level::Ok,
                Text::format(format, std::forward<TArgs>(args)...)
            );
        }

        template <typename... TArgs>
        void finishError(std::format_string<TArgs...> format, TArgs&&... args) {
            if (!active_)
                return;

            close(
                Logger::Level::Exception,
                Text::format(format, std::forward<TArgs>(args)...)
            );
        }

        void cancel() noexcept {
            if (!active_)
                return;

            closeWithoutMessage();
        }

        ~Scope() {
            if (active_)
                finishError("aborted");
        }

    private:
        using Clock = std::chrono::steady_clock;

        void close(Level level, const Text& message) {
            const size_t lineCount = Logger::scopes().back().transientLines;
            --Logger::indent();

            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                Clock::now() - startTime_
            ).count();

            const bool success = level == Level::Ok;
            const bool compact = Logger::consoleMode() == Logger::ConsoleMode::Default;

            Logger::scopes().pop_back();

            if (success) {
                if (compact)
                    eraseLines(lineCount);

                Logger::ok(tag_, message + Text::format("<gr> ({} us)</>", elapsed));
                pendingGap() = compact;
            } else {
                Logger::exception(tag_, message + Text::format("<gr> ({} us)</>", elapsed));
                pendingGap() = false;
            }

            if (Logger::consoleMode() == Logger::ConsoleMode::Verbose || (Logger::consoleMode() == Logger::ConsoleMode::Default && !success))
                Logger::message("");

            active_ = false;
        }

        void closeWithoutMessage() noexcept {
            --Logger::indent();

            const size_t lineCount = Logger::scopes().back().transientLines;
            Logger::scopes().pop_back();

            if (Logger::consoleMode() == Logger::ConsoleMode::Default)
                Logger::eraseLines(lineCount);

            active_ = false;
        }

        std::string tag_;
        std::string finishMessage_;
        Clock::time_point startTime_;
        bool active_;
    };
};