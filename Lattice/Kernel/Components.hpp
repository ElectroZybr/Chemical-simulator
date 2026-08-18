#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <format>
#include <stdexcept>
#include <type_traits>

#include <Lattice/Kernel/TypeName.hpp>
#include <Lattice/Kernel/ModuleRegistry.hpp>
#include <Lattice/Tools/Logger.hpp>

namespace Lattice {

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
    ModuleRegistry* registry = nullptr;

    using ComponentKey = std::pair<std::string, std::string>; // {typeName, instanceName}

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

    explicit Components(ModuleRegistry* registry)
        : registry(registry)
    {}

    Component<Components> addBranch(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<Components>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return Component<Components>{it->second};
        }
        auto component = std::make_unique<ComponentData>();
        Components* obj;
        obj = new Components(registry);

        component->instance = obj;
        component->api = obj;
        component->destroy = [](void* p) { delete static_cast<Components*>(p); };

        ComponentData* ptr = component.get();
        storage.push_back(std::move(component));
        lookup.emplace(std::move(key), ptr);
        Logger::info(moduleName, "+ branch     '{}'", instanceName);
        return Component<Components>{ptr};
    }

    template<typename T>
    Component<T> addComponent(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<T>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return Component<T>{it->second};
        }

        static_assert(!std::is_abstract_v<T>, "addComponent<T>: T must not be abstract");

        auto component = std::make_unique<ComponentData>();
        T* obj;

        if constexpr (std::is_same_v<T, Components>) {
            obj = new Components(registry);
        }
        else if constexpr (std::is_constructible_v<T, Components&>) {
            obj = new T(*this);
        }
        else if constexpr (std::is_default_constructible_v<T>) {
            obj = new T();
        }
        else {
            static_assert(
                std::is_constructible_v<T, Components&> ||
                std::is_default_constructible_v<T>,
                "Component must be constructible with Components& or default constructible"
            );
        }

        component->instance = obj;
        component->api = obj;
        component->destroy = [](void* p) { delete static_cast<T*>(p); };

        ComponentData* ptr = component.get();
        storage.push_back(std::move(component));
        lookup.emplace(std::move(key), ptr);

        Logger::info(moduleName, "+ component  '{}'", typeName<T>());

        return Component<T>{ptr};
    }

    template<typename API>
    Component<API> addInterfaceSlot(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<API>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return Component<API>{it->second};
        }
        auto component = std::make_unique<ComponentData>();
        ComponentData* ptr = component.get();
        storage.push_back(std::move(component));
        lookup.emplace(std::move(key), ptr);

        Logger::info(moduleName, "+ interface  '{}'", typeName<API>());

        return Component<API>{ptr};
    } 

    template<typename API, typename Impl>
    Component<API> useInterface(std::string_view instanceName = "default") {
        return useInterface<API>(typeName<Impl>(), instanceName);
    }

    template<typename API>
    Component<API> useInterface(std::string_view implName, std::string_view instanceName = "default") {
        const auto& implementation = registry->requireImpl<API>(implName);
        auto component = require<API>(instanceName);
        void* instance = implementation.create(this);

        component.data->reset(
            instance,
            implementation.getAPI(instance),
            implementation.destroy
        );

        Logger::info(moduleName, "> use  '{}' = '{}'", typeName<API>(), implName);

        return component;
    }

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
        if (!c.exists()) {
            throw std::runtime_error(std::format(
                "Component '{}' with instance '{}' not found",
                typeName<API>(), instanceName));
        }
        return c;
    }
};

} // namespace Lattice