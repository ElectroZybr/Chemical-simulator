#include "Action.hpp"
#include <Lattice/Kernel/Settings.hpp>
#include <Lattice/Tools/Logger.hpp>


ActionMap::ActionState& ActionMap::ensureAction(const ActionId& id) {
    return actions_[std::string(id)];
}

const ActionMap::ActionState* ActionMap::findAction(std::string_view id) const {
    auto it = actions_.find(std::string(id));
    if (it == actions_.end()) return nullptr;
    return &it->second;
}

void ActionMap::bindAction(std::string_view key, std::string_view trigger, ActionMode mode) {
    auto combo = Input::parseCombo(trigger);
    if (!combo) {
        Logger::warning("ActionMap", "bad trigger '{}' for '{}'", trigger, key);
        return;
    }

    auto cb = settings_.tryHandler(key);
    if (!cb) {
        cb = settings_.tryHandler(std::string("actions.") + std::string(key));
    }
    if (!cb) {
        Logger::warning("ActionMap", "no handler for action '{}'", key);
        return;
    }
    ensureAction(std::string(key));
    bindings_.push_back({std::string(key), *combo, mode, std::move(cb), false});
    Logger::ok("ActionMap", "bound '{}' -> '{}'", key, trigger);
}

void ActionMap::bindToggle(std::string_view key, std::string_view trigger, ActionMode mode) {
    auto combo = Input::parseCombo(trigger);
    if (!combo) {
        Logger::warning("ActionMap", "bad trigger '{}' for '{}'", trigger, key);
        return;
    }

    auto cb = settings_.makeToggle(key);
    bindings_.push_back({std::string(key), *combo, mode, std::move(cb), false});
}

void ActionMap::bindAdd(std::string_view key, std::string_view trigger, double delta, ActionMode mode) {
    auto combo = Input::parseCombo(trigger);
    if (!combo) {
        Logger::warning("ActionMap", "bad trigger '{}' for '{}'", trigger, key);
        return;
    }

    auto cb = settings_.makeAdd(key, delta);
    bindings_.push_back({std::string(key), *combo, mode, std::move(cb), false});
}

void ActionMap::tick(const Input::KeyboardState& kb) {
    for (auto& [_, st] : actions_) {
        st.pressed = false;
        st.released = false;
        st.down = false;
    }

    for (auto& b : bindings_) {
        const bool now = Input::comboDown(b.combo, kb);
        const bool edgePress = now && !b.wasDown;
        const bool edgeRelease = !now && b.wasDown;

        auto& st = ensureAction(b.id);
        if (now) st.down = true;
        if (edgePress) st.pressed = true;
        if (edgeRelease) st.released = true;

        if (b.cb) {
            switch (b.mode) {
            case ActionMode::OnPress:
                if (edgePress) b.cb();
                break;
            case ActionMode::OnHold:
                if (now) b.cb();
                break;
            case ActionMode::OnRelease:
                if (edgeRelease) b.cb();
                break;
            }
        }

        b.wasDown = now;
    }
}

bool ActionMap::down(std::string_view id) const {
    if (const auto* st = findAction(id)) return st->down;
    return false;
}

bool ActionMap::pressed(std::string_view id) const {
    if (const auto* st = findAction(id)) return st->pressed;
    return false;
}

bool ActionMap::released(std::string_view id) const {
    if (const auto* st = findAction(id)) return st->released;
    return false;
}

void ActionMap::clearBinds() {
    bindings_.clear();
    for (auto& [_, st] : actions_) {
        st.down = st.pressed = st.released = false;
    }
}