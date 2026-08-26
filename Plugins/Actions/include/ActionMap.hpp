#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Lattice/Kernel/RefSlot.hpp"
#include "Lattice/Kernel/SubsystemAPI.hpp"

#include "InputAPI.hpp"
#include "CommandSlots.hpp"


namespace Lattice {
    class Components;
    class Settings;
}

enum class ActionMode {
    OnPress,
    OnHold,
    OnRelease
};

class ActionMap final : public SubsystemAPI {
public:
    explicit ActionMap(Lattice::Components& branch);
    void configure(Lattice::Components& branch);

    void bind(std::string_view actionId, std::string_view trigger, ActionMode mode = ActionMode::OnPress);
    void bindToggle(std::string_view valueKey, std::string_view trigger, ActionMode mode = ActionMode::OnPress);
    void bindAdd(std::string_view valueKey, std::string_view trigger, double delta, ActionMode mode = ActionMode::OnPress);

    void tick();

    void set(std::string_view group, std::string_view action);
    void set(std::string_view id);

    bool down(std::string_view id) const;
    bool pressed(std::string_view id) const;
    bool released(std::string_view id) const;

    void clearBinds();

private:
    struct Binding {
        std::string id;
        std::string trigger;
        ActionMode mode = ActionMode::OnPress;
        CommandSlot& slot;
        bool wasDown = false;
    };

    struct ActionState {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };

    bool pushBinding(std::string id, std::string_view trigger, ActionMode mode, CommandSlot& slot);
    ActionState& ensureAction(std::string_view id);
    const ActionState* findAction(std::string_view id) const;
    
    std::unordered_map<std::string, ActionState> actions_;

    std::vector<InputAPI*> inputs_;
    std::vector<Binding> bindings_;
    Ref<CommandSlots> slots_;
    Ref<Lattice::Settings> settings_;
};