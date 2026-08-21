#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <format>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <Lattice/Kernel/TypeName.hpp>
#include <Lattice/Kernel/Registry.hpp>
#include <Lattice/Tools/Logger.hpp>
#include "Lattice/Tools/LogStyle.hpp"

namespace Lattice {

enum class Mode {
    Normal,
    Check
};

template<typename T>
concept HasConfigure = requires(T& t) {
    t.configure();
};

struct Requirement {
    std::string type;
    std::string instance;
    int depth = 0;
};

struct ComponentData {
    void* instance = nullptr;
    void* api = nullptr;
    void (*destroy)(void*) = nullptr;

    ~ComponentData() {
        if (instance && destroy)
            destroy(instance);
    }

    void reset(void* newInstance, void* newApi, void (*newDestroy)(void*)) {
        if (instance && destroy)
            destroy(instance);
        instance = newInstance;
        api = newApi;
        destroy = newDestroy;
    }
};

template<typename API>
struct Component {
    ComponentData* data = nullptr;

    Component() = default;
    explicit Component(ComponentData* d) : data(d) {}

    API* operator->() const {
        return data ? static_cast<API*>(data->api) : nullptr;
    }

    API& operator*() const {
        return *static_cast<API*>(data->api);
    }

    explicit operator bool() const {
        return data && data->api;
    }

    API* get() const {
        return data ? static_cast<API*>(data->api) : nullptr;
    }

    bool exists() const {
        return data != nullptr;
    }

    bool ready() const {
        return data && data->api;
    }
};

class Components {
private:
    Registry* registry = nullptr;
    Components* parent = nullptr;
    std::vector<Requirement> reqs;
    Mode mode = Mode::Normal;
    std::string name;
    int depth = 0;

    using ComponentKey = std::pair<std::string, std::string>;

    struct ComponentKeyHash {
        size_t operator()(const ComponentKey& k) const noexcept {
            size_t h1 = std::hash<std::string>{}(k.first);
            size_t h2 = std::hash<std::string>{}(k.second);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    std::unordered_map<ComponentKey, ComponentData*, ComponentKeyHash> lookup;
    // владеет объектами, нужна для уничтожения в правильном порядке
    std::vector<std::unique_ptr<ComponentData>> storage;

public:
    static constexpr std::string_view moduleName = "Components";

    explicit Components(Registry* registry, Components* parent = nullptr, Mode mode = Mode::Normal, std::string_view name = "Root")
        : registry(registry), parent(parent), mode(mode), name(name) {

        if (mode == Mode::Check) {
            reqs.push_back({std::string(name), std::string(name), depth});
            ++depth;
        }
    }

    // -------------------------------------------------------------------------

    Component<Components> addBranch(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<Components>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return Component<Components>{it->second};
        }

        if (mode == Mode::Check) {
            reqs.push_back({std::string(instanceName), std::string(instanceName), depth});
            ++depth;
        }

        auto component = std::make_unique<ComponentData>();
        Components* obj = new Components(registry, this, mode, instanceName);
        component->instance = obj;
        component->api = obj;
        component->destroy = [](void* p) { delete static_cast<Components*>(p); };

        ComponentData* ptr = component.get();
        storage.push_back(std::move(component));
        lookup.emplace(std::move(key), ptr);

        if (mode == Mode::Normal) {
            Logger::info(moduleName, "+ branch '{}'", instanceName);
        }

        return Component<Components>{ptr};
    }

    // -------------------------------------------------------------------------

    template<typename T>
    Component<T> addComponent(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<T>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return Component<T>{it->second};
        }

        // В Check-режиме всегда записываем зависимость
        if (mode == Mode::Check) {
            reqs.push_back({std::string(typeName<T>()), std::string(instanceName), depth});
        }

        // Если типа нет в реестре
        if (!registry->has(std::string(typeName<T>()))) {
            if (mode == Mode::Check) {
                return Component<T>{nullptr};
            }
            throw std::runtime_error(std::format(
                "Component '{}' not found", typeName<T>()));
        }

        if (mode == Mode::Check) {
            ++depth;
        }

        auto component = std::make_unique<ComponentData>();
        T* obj = nullptr;

        if constexpr (std::is_same_v<T, Components>) {
            obj = new Components(registry, this, mode, instanceName);
        }
        else if constexpr (std::is_constructible_v<T, Components&>) {
            obj = new T(*this);
        }
        else if constexpr (std::is_default_constructible_v<T>) {
            obj = new T();
        }
        else {
            static_assert(
                std::is_constructible_v<T, Components&> || std::is_default_constructible_v<T>,
                "Component must be constructible with Components& or default constructible"
            );
        }

        if (mode == Mode::Check) {
            --depth;
        }

        if (mode == Mode::Normal) {
            if constexpr (HasConfigure<T>) {
                obj->configure();
            }
            Logger::info(moduleName, "+ component '{}'", typeName<T>());
        }

        component->instance = obj;
        component->api = obj;
        component->destroy = [](void* p) { delete static_cast<T*>(p); };

        ComponentData* ptr = component.get();
        storage.push_back(std::move(component));
        lookup.emplace(std::move(key), ptr);

        return Component<T>{ptr};
    }

    // -------------------------------------------------------------------------

    template<typename API>
    Component<API> addInterfaceSlot(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<API>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return Component<API>{it->second};
        }

        if (mode == Mode::Check) {
            reqs.push_back({std::string(typeName<API>()), std::string(instanceName), depth});
            return Component<API>{nullptr};
        }

        auto component = std::make_unique<ComponentData>();
        ComponentData* ptr = component.get();
        storage.push_back(std::move(component));
        lookup.emplace(std::move(key), ptr);

        Logger::info(moduleName, "+ interface '{}'", typeName<API>());
        return Component<API>{ptr};
    }

    // -------------------------------------------------------------------------

    template<typename API, typename Impl>
    Component<API> useInterface(std::string_view instanceName = "default") {
        return useInterface<API>(typeName<Impl>(), instanceName);
    }

    template<typename API>
    Component<API> useInterface(std::string_view implName, std::string_view instanceName = "default") {
        if (mode == Mode::Check) {
            reqs.push_back({std::string(typeName<API>()), std::string(instanceName), depth});
            if (!registry->hasImpl<API>(implName)) {
                Logger::error(moduleName, "unknown implementation '{}' for '{}'", implName, typeName<API>());
                return Component<API>{nullptr};
            }
            const auto& implementation = registry->requireImpl<API>(implName);
            void* instance = implementation.create(this);
            implementation.destroy(instance);
            return Component<API>{nullptr};
        }

        const auto& implementation = registry->requireImpl<API>(implName);
        auto component = require<API>(instanceName);

        void* instance = implementation.create(this);
        component.data->reset(
            instance,
            implementation.getAPI(instance),
            implementation.destroy
        );

        Logger::info(moduleName, "> use '{}' = '{}'", typeName<API>(), implName);
        return component;
    }

    // -------------------------------------------------------------------------

    template<typename API>
    Component<API> get(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<API>()), std::string(instanceName)};
        auto it = lookup.find(key);
        if (it == lookup.end() || !it->second)
            return {};
        return Component<API>{it->second};
    }

    template<typename API>
    Component<API> require(std::string_view instanceName = "default") {
        auto c = get<API>(instanceName);
        if (mode == Mode::Check) {
            reqs.push_back({std::string(typeName<API>()), std::string(instanceName), depth});
            if (c.exists()) {
                return c;
            }
            return Component<API>{nullptr};
        }
        if (!c.exists()) {
            throw std::runtime_error(std::format(
                "Component '{}' with instance '{}' not found",
                typeName<API>(), instanceName));
        }
        return c;
    }

    // -------------------------------------------------------------------------

    std::vector<Requirement> getUniqueRequirements() const {
        // Возвращаем уникальные требования по всему дереву (без дублей), сохраняя порядок
        std::vector<Requirement> result;
        std::unordered_set<std::string> seen;

        for (const auto& r : reqs) {
            std::string key = r.type + "::" + r.instance;
            if (seen.find(key) != seen.end())
                continue;
            seen.insert(key);
            result.push_back(r);
        }

        return result;
    }

    void printRequirements() const {
        Logger::Tree tree("Requirements");
        for (const auto& r : getUniqueRequirements()) {
            tree.node(std::format("{}{}", registry->has(r.type)
                ? Color::paint("✓ ", Color::ok)
                : Color::paint("✗ ", Color::error), r.type),
                0
            );
        }
        tree.print();
    }

    void printRequirementTree() const {
        Logger::Tree tree("Requirements");

        for (const auto& r : reqs) {
            tree.node(std::format("{}{}", registry->has(r.type)
                ? Color::paint("✓ ", Color::ok) 
                : Color::paint("✗ ", Color::error), r.type),
                r.depth
            );
        }
        tree.print();
    }

    const std::vector<Requirement>& getRequirementTree() const {
        return reqs;
    }
};

} // namespace Lattice