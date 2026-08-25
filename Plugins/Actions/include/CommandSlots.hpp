#pragma once

#include <string>
#include <functional>
#include <string_view>
#include <unordered_map>


using Command = std::function<void()>;

class CommandSlot {
public:
    void set(Command command) {
        command_ = std::move(command);
    }
    void clear() {
        command_ = nullptr;
    }
    bool empty() const {
        return !command_;
    }
    void invoke() {
        if (command_)
            command_();
    }

private:
    Command command_ = nullptr;
};


class CommandSlots {
public:
    CommandSlot& addSlot(std::string_view id) {
        return slots_[std::string(id)];
    }

    CommandSlot* getSlot(std::string_view id) {
        auto it = slots_.find(std::string(id));
        return it != slots_.end() ? &it->second : nullptr;
    }

    void set(std::string_view id, Command command) {
        slots_[std::string(id)].set(std::move(command));
    }

    void clear() {
        slots_.clear();
    }

private:
    std::unordered_map<std::string, CommandSlot> slots_;
};