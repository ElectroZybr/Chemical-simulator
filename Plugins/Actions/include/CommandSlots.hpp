#pragma once

#include <string>
#include <functional>
#include <string_view>
#include <unordered_map>

#include <Lattice/Kernel/Node.hpp>

struct CommandContext {
    Lattice::Node* branch;

    CommandContext& operator=(Lattice::Node* value) {
        branch = value;
        return *this;
    }
};


using Command = std::function<void()>;

class CommandSlot {
public:
    void set(Command command) {//, CommandContext context
        command_ = std::move(command);
        // context_ = context;
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
    Command command_;
    // CommandContext context_;
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

    void set(std::string_view id, Command command) {//, CommandContext ctx
        slots_[std::string(id)].set(std::move(command));
    }

    void clear() {
        slots_.clear();
    }

private:
    std::unordered_map<std::string, CommandSlot> slots_;
};