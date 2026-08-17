#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <format>
#include <stdexcept>
#include <type_traits>

#include "Lattice/Kernel/TypeName.hpp"
#include "Lattice/Kernel/ModuleRegistry.hpp"

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
    explicit Components(ModuleRegistry* registry)
        : registry(registry)
    {}

    template<typename API>
    Component<API> add(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<API>()), std::string(instanceName)};
        if (auto it = lookup.find(key); it != lookup.end()) {
            return Component<API>{it->second};
        }
        auto component = std::make_unique<ComponentData>();

        if constexpr (std::is_same_v<API, Components>) {
            auto* obj = new Components(registry);
            component->instance = obj;
            component->api = obj;
            component->destroy = [](void* p) { delete static_cast<Components*>(p); };
        }
        else if constexpr (std::is_default_constructible_v<API> && !std::is_abstract_v<API>) {
            auto* obj = new API();
            component->instance = obj;
            component->api = obj;
            component->destroy = [](void* p) { delete static_cast<API*>(p); };
        }
        ComponentData* ptr = component.get();
        storage.push_back(std::move(component));
        lookup.emplace(std::move(key), ptr);

        return Component<API>{ptr};
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
        if (!c) {
            throw std::runtime_error(std::format(
                "Component '{}' with instance '{}' not found",
                typeName<API>(), instanceName));
        }
        return c;
    }

    template<typename API, typename Impl>
    Component<API> use(std::string_view instanceName = "default") {
        return use<API>(typeName<Impl>(), instanceName);
    }

    template<typename API>
    Component<API> use(std::string_view implName, std::string_view instanceName = "default") {
        const auto& implementation = registry->requireImpl<API>(implName);
        auto component = add<API>(instanceName);
        void* instance = implementation.create(this);

        component.data->reset(
            instance,
            implementation.getAPI(instance),
            implementation.destroy
        );
        return component;
    }
};

} // namespace Lattice