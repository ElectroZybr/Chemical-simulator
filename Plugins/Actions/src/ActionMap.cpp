#include "ActionMap.hpp"
#include "CommandSlots.hpp"

#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>
#include <Lattice/Tools/Logger.hpp>


ActionMap::ActionMap(Lattice::Components& branch) {
    branch.add<CommandSlots>();
}

void ActionMap::configure(Lattice::Components& branch) {
    settings_ = branch.require<Lattice::Settings>();
    slots_ = branch.require<CommandSlots>();
    // находим все инпуты (устройства ввода)
    inputs_ = branch.globalCollect<InputAPI>();
}

ActionMap::ActionState& ActionMap::ensureAction(std::string_view id) {
    return actions_[std::string(id)];
}

const ActionMap::ActionState* ActionMap::findAction(std::string_view id) const {
    auto it = actions_.find(std::string(id));
    return it == actions_.end() ? nullptr : &it->second;
}

bool ActionMap::pushBinding(std::string id, std::string_view trigger, ActionMode mode, CommandSlot& slot) {
    ensureAction(id);
    bindings_.push_back({std::move(id), std::string(trigger), mode, slot});
    return true;
}

void ActionMap::set(std::string_view group, std::string_view action) {
    set(std::string(group) + "." + std::string(action));
}

void ActionMap::set(std::string_view id) {
    auto cb = settings_->tryHandler(id);

    if (!cb) {
        Logger::warning("ActionMap", "no handler for '{}'", id);
        return;
    }

    slots_->set(id, std::move(cb));

    Logger::info("ActionMap", "set '{}'", id);
}

void ActionMap::bind(std::string_view id, std::string_view trigger, ActionMode mode) {
    auto* slot = slots_->getSlot(id);
    if (!slot) {
        Logger::warning("ActionMap", "slot '{}' not found", id);
        return;
    }

    if (pushBinding(std::string(id), trigger, mode, *slot))
        Logger::ok("ActionMap", "bound '{}' -> '{}'", id, trigger);
}

void ActionMap::bindToggle(std::string_view id, std::string_view trigger, ActionMode mode) {
    if (!settings_->hasValue(id)) {
        Logger::warning("ActionMap", "toggle target missing '{}'", id);
        return;
    }

    try {
        slots_->addSlot(id).set(settings_->makeToggle(id));
        bind(id, trigger, mode);
    } catch (const std::exception& e) {
        Logger::warning("ActionMap", "toggle '{}': {}", id, e.what());
    }
}

void ActionMap::bindAdd(std::string_view id, std::string_view trigger, double delta, ActionMode mode) {
    try {
        slots_->addSlot(id).set(settings_->makeAdd(id, delta));
        bind(id, trigger, mode);
    } catch (const std::exception& e) {
        Logger::warning("ActionMap", "add '{}': {}", id, e.what());
    }
}

void ActionMap::tick() {
    for (auto& [_, state] : actions_) {
        state.down = false;
        state.pressed = false;
        state.released = false;
    }

    for (auto& binding : bindings_) {
        bool now = false;

        for (auto* input : inputs_) {
            if (input && input->down(binding.trigger)) {
                now = true;
                break;
            }
        }

        const bool pressed = now && !binding.wasDown;
        const bool released = !now && binding.wasDown;

        // Logger::info("ActionMap",
        //     "binding '{}' <- '{}' now={} pressed={} released={}",
        //     binding.id, binding.trigger, now, pressed, released);

        auto& state = ensureAction(binding.id);

        state.down |= now;
        state.pressed |= pressed;
        state.released |= released;

        switch (binding.mode) {
        case ActionMode::OnPress:
            if (pressed) {
                Logger::info("ActionMap",
                    "invoke '{}' <- '{}' [OnPress]",
                    binding.id, binding.trigger);
                binding.slot.invoke();
            }
            break;

        case ActionMode::OnHold:
            if (now) {
                Logger::info("ActionMap",
                    "invoke '{}' <- '{}' [OnHold]",
                    binding.id, binding.trigger);
                binding.slot.invoke();
            }
            break;

        case ActionMode::OnRelease:
            if (released) {
                Logger::info("ActionMap",
                    "invoke '{}' <- '{}' [OnRelease]",
                    binding.id, binding.trigger);
                binding.slot.invoke();
            }
            break;
        }

        binding.wasDown = now;
    }
}

bool ActionMap::down(std::string_view id) const {
    if (const auto* state = findAction(id))
        return state->down;
    return false;
}

bool ActionMap::pressed(std::string_view id) const {
    if (const auto* state = findAction(id))
        return state->pressed;
    return false;
}

bool ActionMap::released(std::string_view id) const {
    if (const auto* state = findAction(id))
        return state->released;
    return false;
}

void ActionMap::clearBinds() {
    bindings_.clear();

    for (auto& [_, state] : actions_)
        state = {};
}