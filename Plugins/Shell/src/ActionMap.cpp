#include "ActionMap.hpp"

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

struct ParsedTrigger {
    enum class Kind { Key, MouseButton } kind{};
    Input::KeyCombo combo{};
    Input::MouseButton button = Input::MouseButton::Count;
};

static std::optional<ParsedTrigger> parseTrigger(std::string_view trigger) {
    // 1) мышь
    if (auto btn = Input::mouseButtonFromString(trigger);
        btn != Input::MouseButton::Count) {
        ParsedTrigger t;
        t.kind = ParsedTrigger::Kind::MouseButton;
        t.button = btn;
        return t;
    }
    // 2) клавиатура
    if (auto combo = Input::parseCombo(trigger)) {
        ParsedTrigger t;
        t.kind = ParsedTrigger::Kind::Key;
        t.combo = *combo;
        return t;
    }
    return std::nullopt;
}

bool ActionMap::pushBinding(std::string id, std::string_view trigger, ActionMode mode, std::function<void()> cb) {
    if (!cb) {
        Logger::warning("ActionMap", "empty callback for '{}'", id);
        return false;
    }
    auto tr = parseTrigger(trigger);
    if (!tr) {
        Logger::warning("ActionMap", "bad trigger '{}' for '{}'", trigger, id);
        return false;
    }

    Binding b;
    b.id = std::move(id);
    b.mode = mode;
    b.cb = std::move(cb);
    b.wasDown = false;

    if (tr->kind == ParsedTrigger::Kind::Key) {
        b.kind = Binding::Kind::Key;
        b.combo = tr->combo;
    } else {
        b.kind = Binding::Kind::MouseButton;
        b.button = tr->button;
    }

    ensureAction(b.id);
    bindings_.push_back(std::move(b));
    return true;
}

void ActionMap::bindAction(std::string_view key, std::string_view trigger, ActionMode mode) {
    auto cb = settings_.tryHandler(key);
    if (!cb)
        cb = settings_.tryHandler(std::string("actions.") + std::string(key));
    if (!cb) {
        Logger::warning("ActionMap", "no handler for action '{}'", key);
        return;
    }
    if (pushBinding(std::string(key), trigger, mode, std::move(cb)))
        Logger::ok("ActionMap", "bound action '{}' -> '{}'", key, trigger);
}

void ActionMap::bindToggle(std::string_view key, std::string_view trigger, ActionMode mode) {
    if (!settings_.hasValue(key)) {
        Logger::warning("ActionMap", "toggle target missing '{}'", key);
        return;
    }
    try {
        auto cb = settings_.makeToggle(key);
        if (pushBinding(std::string(key), trigger, mode, std::move(cb)))
            Logger::ok("ActionMap", "bound toggle '{}' -> '{}'", key, trigger);
    } catch (const std::exception& e) {
        Logger::warning("ActionMap", "toggle '{}': {}", key, e.what());
    }
}

void ActionMap::bindAdd(std::string_view key, std::string_view trigger,
                        double delta, ActionMode mode) {
    try {
        auto cb = settings_.makeAdd(key, delta);
        if (pushBinding(std::string(key), trigger, mode, std::move(cb)))
            Logger::ok("ActionMap", "bound add '{}' ({:+}) -> '{}'", key, delta, trigger);
    } catch (const std::exception& e) {
        Logger::warning("ActionMap", "add '{}': {}", key, e.what());
    }
}

void ActionMap::tick(const Input::KeyboardState& kb, const Input::MouseState& mouse) {
    for (auto& [_, st] : actions_) {
        st.pressed = false;
        st.released = false;
        st.down = false;
    }

    for (auto& b : bindings_) {
        bool now = false;
        if (b.kind == Binding::Kind::Key)
            now = Input::comboDown(b.combo, kb);
        else
            now = mouse.isDown(b.button);

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