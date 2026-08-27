#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <mutex>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <Lattice/Tools/LogStyle.hpp>
#include <Lattice/Tools/Text.hpp>


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
        Exception,
        Ok,
    };

    static std::filesystem::path logPath() {
        return std::filesystem::path("Logs") / "lattice.log";
    }

    static void setConsoleMode(ConsoleMode mode) noexcept;
    static ConsoleMode consoleMode() noexcept;

    template <typename... TArgs>
    static void message(std::format_string<TArgs...> format, TArgs&&... args) {
        print(Text::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void action(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Action, tag, Text::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void trace(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Trace, tag, Text::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void debug(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Debug, tag, Text::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void info(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Info, tag, Text::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void warning(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Warning, tag, Text::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void error(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Error, tag, Text::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void exception(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Exception, tag, Text::format(format, std::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    static void ok(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args) {
        print(Level::Ok, tag, Text::format(format, std::forward<TArgs>(args)...));
    }

    static void treeLine(std::string_view message);

    template <typename... TArgs>
    static void tree(std::format_string<TArgs...> format, TArgs&&... args) {
        treeLine(std::format(format, std::forward<TArgs>(args)...));
    }

private:
    static void print(const Text& text);
    static void print(Level level, std::string_view tag, const Text& text);
    static std::mutex& mutex();

public:
    class Tree {
    public:
        class Node {
        public:
            explicit Node(std::string name)
                : name_(std::move(name)) {}

            Node& branch(std::string_view name) {
                children_.push_back(
                    std::make_unique<Node>(std::string(name))
                );

                return *children_.back();
            }

            void node(std::string_view name) {
                children_.push_back(
                    std::make_unique<Node>(std::string(name))
                );
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
            while (parents_.size() > depth) {
                parents_.pop_back();
            }

            Node* parent;

            if (parents_.empty()) {
                parent = &root_;
            } else {
                parent = parents_.back();
            }

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

        // Только для построения.
        std::vector<Node*> parents_;
    };

    class Scope {
    public:
        template <typename... TArgs>
        Scope(std::string_view tag,
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
        Scope(std::string_view tag,
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

                template <typename... TArgs>
        void finishError(std::format_string<TArgs...> format, TArgs&&... args) {
            if (!active_) return;
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                Clock::now() - startTime_).count();
            Logger::exception(tag_, "{} ({} us)", std::format(format, std::forward<TArgs>(args)...), elapsed);
            active_ = false;
        }

        void cancel() noexcept { active_ = false; }

        ~Scope() {
            if (active_)
                finishError("aborted");
        }


    private:
        using Clock = std::chrono::steady_clock;
        std::string tag_;
        std::string finishMessage_;
        Clock::time_point startTime_;
        bool active_;
    };
};
