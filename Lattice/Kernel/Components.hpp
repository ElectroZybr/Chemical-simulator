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

// Хендл на слот в мешке: reset() меняет impl, все Slot<API> это видят.
// Конкретные типы хранить как T* (add/require), интерфейсы — Slot<API> (use/get).
template<typename API>
struct Slot {
    ComponentData* data = nullptr;

    Slot() = default;
    explicit Slot(ComponentData* d) : data(d) {}

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

    Components* addBranch(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<Components>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return static_cast<Components*>(it->second->api);
        }

        if (mode == Mode::Check) {
            reqs.push_back({std::string(instanceName), std::string(instanceName), depth});
            ++depth;
        }

        auto data = std::make_unique<ComponentData>();
        Components* obj = new Components(registry, this, mode, instanceName);
        data->instance = obj;
        data->api = obj;
        data->destroy = [](void* p) { delete static_cast<Components*>(p); };

        ComponentData* ptr = data.get();
        storage.push_back(std::move(data));
        lookup.emplace(std::move(key), ptr);

        if (mode == Mode::Normal) {
            Logger::info(moduleName, "+ branch '{}'", instanceName);
        }

        return obj;
    }

    template<typename T>
    T* add(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<T>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return static_cast<T*>(it->second->api);
        }

        if (mode == Mode::Check) {
            reqs.push_back({std::string(typeName<T>()), std::string(instanceName), depth});
        }

        const Registry::TypeEntry* entry = registry->find(std::string(typeName<T>()));
        if (!entry || !entry->create) {
            if (mode == Mode::Check) {
                return nullptr;
            }
            throw std::runtime_error(std::format(
                "Component '{}' not found", typeName<T>()));
        }

        if (mode == Mode::Check) {
            ++depth;
        }

        auto data = std::make_unique<ComponentData>();
        void* instance = entry->create(this);
        T* obj = static_cast<T*>(instance);

        if (mode == Mode::Check) {
            --depth;
        }

        if (mode == Mode::Normal) {
            if (entry->configure)
                entry->configure(obj);
            Logger::info(moduleName, "+ {}", typeName<T>());
        }

        data->instance = instance;
        data->api = instance;
        data->destroy = entry->destroy;

        ComponentData* ptr = data.get();
        storage.push_back(std::move(data));
        lookup.emplace(std::move(key), ptr);

        return obj;
    }

    template<typename API, typename Impl>
    Slot<API> use(std::string_view instanceName = "default") {
        return use<API>(typeName<Impl>(), instanceName);
    }

    template<typename API>
    Slot<API> use(std::string_view implName, std::string_view instanceName = "default") {
        if (mode == Mode::Check) {
            reqs.push_back({std::string(typeName<API>()), std::string(instanceName), depth});
            if (!registry->hasImpl<API>(implName)) {
                Logger::error(moduleName, "unknown implementation '{}' for '{}'", implName, typeName<API>());
                return {};
            }
            // TODO: Check не должен create/destroy настоящие объекты
            const auto& implementation = registry->requireImpl<API>(implName);
            void* instance = implementation.create(this);
            implementation.destroy(instance);
            return {};
        }

        const auto& implementation = registry->requireImpl<API>(implName);
        Slot<API> slot = get<API>(instanceName);
        if (!slot.exists()) {
            auto data = std::make_unique<ComponentData>();
            ComponentData* ptr = data.get();
            storage.push_back(std::move(data));
            lookup.emplace(
                ComponentKey{std::string(typeName<API>()), std::string(instanceName)},
                ptr
            );
            slot = Slot<API>{ptr};
            Logger::info(moduleName, "+ interface '{}'", typeName<API>());
        }

        void* instance = implementation.create(this);
        slot.data->reset(
            instance,
            implementation.getAPI(instance),
            implementation.destroy
        );

        if (implementation.configure)
            implementation.configure(instance);

        Logger::info(moduleName, "> use '{}' = '{}'", typeName<API>(), implName);
        return slot;
    }

    template<typename API>
    Slot<API> get(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<API>()), std::string(instanceName)};
        auto it = lookup.find(key);
        if (it == lookup.end() || !it->second)
            return {};
        return Slot<API>{it->second};
    }

    template<typename T>
    T* require(std::string_view instanceName = "default") {
        Slot<T> slot = get<T>(instanceName);
        if (mode == Mode::Check) {
            reqs.push_back({std::string(typeName<T>()), std::string(instanceName), depth});
            return slot.get();
        }
        if (!slot.exists()) {
            throw std::runtime_error(std::format(
                "Component '{}' with instance '{}' not found",
                typeName<T>(), instanceName));
        }
        return slot.get();
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