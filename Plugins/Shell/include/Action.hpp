#pragma once

#include "Keyboard.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Lattice { class Settings; }

enum class ActionMode {
    OnPress,   // single
    OnHold,    // while pressed
    OnRelease  // optional
};

class ActionMap {
public:
    using ActionId = std::string_view ;
    using Handler  = std::function<void()>;
    ActionMap(Lattice::Settings& settings) : settings_(settings) {}

    void bindAction(std::string_view actionId, std::string_view trigger, ActionMode mode = ActionMode::OnPress);
    void bindToggle(std::string_view valueKey, std::string_view trigger, ActionMode mode = ActionMode::OnPress);
    void bindAdd(std::string_view valueKey, std::string_view trigger, double delta, ActionMode mode = ActionMode::OnPress);

    // вызывать раз за кадр после keyboard.beginFrame + poll
    void tick(const Input::KeyboardState& kb);

    bool down(std::string_view id) const;
    bool pressed(std::string_view id) const;
    bool released(std::string_view id) const;

    void clearBinds();

private:
    struct Binding {
        std::string id;
        Input::KeyCombo combo;
        ActionMode mode = ActionMode::OnPress;
        std::function<void()> cb;
        bool wasDown = false;
    };

    struct ActionState {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };

    std::vector<Binding> bindings_;
    std::unordered_map<std::string, ActionState> actions_;

    ActionState& ensureAction(const ActionId& id);
    const ActionState* findAction(std::string_view id) const;

    Lattice::Settings& settings_;
};