#pragma once

#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Exception.hpp>

#include "Document.hpp"
#include "LoaderAPI.hpp"
#include "ActionMap.hpp"

class KeybindsLoader final : public LoaderAPI {
public:
    void configure(Lattice::Components& branch) {
        actionMap = branch.require<ActionMap>();
    }

    void load(const Document& doc) override {
        throw Lattice::Exception("KeybindsLoader", "загрузка: о нет всему пизда!!!");
        const auto* keybinds = doc.get("keybinds");

        if (!keybinds || !keybinds->is<Table>())
            return;

        const auto applyTable =
            [this](const Table& table, const std::string& prefix,
                auto&& self) -> void
        {
            for (const auto& [key, value] : table) {

                const std::string path =
                    prefix.empty()
                        ? key
                        : prefix + "." + key;

                if (value.is<Table>()) {
                    self(value.as<Table>(), path, self);
                    continue;
                }

                std::vector<Value> args;

                if (value.is<Array>())
                    args = value.as<Array>();
                else
                    args.push_back(value);

                if (args.empty())
                    continue;

                const auto* trigger =
                    std::get_if<std::string>(&args[0]);

                if (!trigger)
                    continue;

                std::string op = "action";
                double delta = 0.0;
                ActionMode mode = ActionMode::OnPress;

                for (size_t i = 1; i < args.size(); ++i) {

                    const auto* arg =
                        std::get_if<std::string>(&args[i]);

                    if (!arg)
                        continue;

                    if (*arg == "toggle") {
                        op = "toggle";
                    }
                    else if (*arg == "add") {
                        op = "add";

                        if (i + 1 < args.size()) {
                            if (const auto* number =
                                    std::get_if<double>(&args[++i]))
                            {
                                delta = *number;
                            }
                        }
                    }
                    else if (*arg == "hold") {
                        mode = ActionMode::OnHold;
                    }
                    else if (*arg == "press") {
                        mode = ActionMode::OnPress;
                    }
                }

                if (op == "toggle")
                    actionMap->bindToggle(path, *trigger, mode);
                else if (op == "add")
                    actionMap->bindAdd(path, *trigger, delta, mode);
                else
                    actionMap->bind(path, *trigger, mode);
            }
        };

        applyTable(keybinds->as<Table>(), "", applyTable);
    }

private:
    Ref<ActionMap> actionMap;
};