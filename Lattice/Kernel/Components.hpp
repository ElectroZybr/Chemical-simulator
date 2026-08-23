#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <format>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <Lattice/Kernel/TypeName.hpp>
#include <Lattice/Kernel/Registry.hpp>
#include <Lattice/Kernel/Requirements.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Tools/Logger.hpp>

namespace Lattice {

class Components;

struct ComponentData {
    Components* owner = nullptr;
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

// Хендл на слот узла. get/require идут вверх по дереву к корню.
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
    using ComponentKey = std::pair<std::string, std::string>;

    struct ComponentKeyHash {
        size_t operator()(const ComponentKey& k) const noexcept {
            size_t h1 = std::hash<std::string>{}(k.first);
            size_t h2 = std::hash<std::string>{}(k.second);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    Registry* registry = nullptr;
    Components* parent = nullptr;
    std::string name;
    std::string kind;

    std::unordered_map<ComponentKey, ComponentData*, ComponentKeyHash> lookup;
    std::vector<std::unique_ptr<Components>> children;
    ComponentData self;

    template<typename API>
    Slot<API> getLocal(std::string_view instanceName) {
        auto it = lookup.find(ComponentKey{std::string(typeName<API>()), std::string(instanceName)});
        if (it == lookup.end() || !it->second)
            return {};
        return Slot<API>{it->second};
    }

    Components* nodeOf(ComponentData* data) const {
        return data ? data->owner : nullptr;
    }

    void index(const std::string& type, std::string_view instanceName, ComponentData* data) {
        lookup.insert_or_assign(ComponentKey{type, std::string(instanceName)}, data);
    }

    Components* makeChild(std::string_view instanceName, std::string_view childKind = {}) {
        auto node = std::make_unique<Components>(registry, this, instanceName);
        Components* raw = node.get();
        raw->self.owner = raw;
        raw->kind = childKind;
        children.push_back(std::move(node));
        return raw;
    }

    std::string label() const {
        if (kind.empty())
            return name.empty() ? "Root" : name;
        if (name.empty() || name == "default")
            return kind;
        return std::format("{} '{}'", kind, name);
    }

    void appendTree(Logger::Tree& tree, size_t depth) const {
        for (const auto& child : children) {
            tree.node(child->label(), depth);
            child->appendTree(tree, depth + 1);
        }
    }

    void clearChildren() {
        children.clear();
    }

    void stopServices() {
        const std::string apiName{typeName<ServiceAPI>()};
        for (auto& [key, data] : lookup) {
            if (key.first != apiName || !data || !data->api)
                continue;
            static_cast<ServiceAPI*>(data->api)->stop();
        }
        for (auto& child : children)
            child->stopServices();
    }

public:
    static constexpr std::string_view moduleName = "Components";

    explicit Components(Registry* registry, Components* parent = nullptr, std::string_view name = "Root")
        : registry(registry), parent(parent), name(name) {
        self.owner = this;
    }

    Components(const Components&) = delete;
    Components& operator=(const Components&) = delete;
    Components(Components&&) = delete;
    Components& operator=(Components&&) = delete;

    ~Components();

    Components* addBranch(std::string_view instanceName = "default") {
        if (auto existing = getLocal<Components>(instanceName); existing.exists())
            return nodeOf(existing.data);

        Components* child = makeChild(instanceName, "branch");
        index(std::string(typeName<Components>()), instanceName, &child->self);

        Logger::info(moduleName, "+ branch '{}'", instanceName);

        return child;
    }

    template<typename T>
    T* add(std::string_view instanceName = "default") {
        noteAdd<T>();
        if (auto existing = getLocal<T>(instanceName))
            return existing.get();

        const Registry::TypeEntry* entry = registry->find(std::string(typeName<T>()));
        if (!entry || !entry->create) {
            throw std::runtime_error(std::format(
                "Component '{}' not found", typeName<T>()));
        }

        Components* child = makeChild(instanceName, typeName<T>());
        void* instance = entry->create(child);
        child->self.reset(instance, instance, entry->destroy);
        index(std::string(typeName<T>()), instanceName, &child->self);
        child->index(std::string(typeName<T>()), instanceName, &child->self);

        if (entry->configure)
            entry->configure(instance);

        Logger::info(moduleName, "+ {}", typeName<T>());

        return static_cast<T*>(instance);
    }

    template<typename API, typename Impl>
    Slot<API> use(std::string_view instanceName = "default") {
        noteUseImpl<API, Impl>();
        return use<API>(typeName<Impl>(), instanceName);
    }

    template<typename API>
    Slot<API> use(std::string_view implName, std::string_view instanceName = "default") {
        noteUse<API>();

        if (!registry->hasImpl<API>(implName)) {
            Logger::error(moduleName, "unknown implementation '{}' for '{}'", implName, typeName<API>());
            throw std::runtime_error(std::format(
                "Implementation '{}' not found for '{}'", implName, typeName<API>()));
        }

        const auto& implementation = registry->requireImpl<API>(implName);
        Components* child = nullptr;

        if (auto existing = getLocal<API>(instanceName); existing.exists()) {
            child = nodeOf(existing.data);
            if (child->self.api) {
                if constexpr (std::is_same_v<API, ServiceAPI>)
                    static_cast<ServiceAPI*>(child->self.api)->stop();
                child->self.reset(nullptr, nullptr, nullptr);
            }
            child->clearChildren();
        } else {
            child = makeChild(instanceName, typeName<API>());
            index(std::string(typeName<API>()), instanceName, &child->self);
            child->index(std::string(typeName<API>()), instanceName, &child->self);
            Logger::info(moduleName, "+ interface '{}'", typeName<API>());
        }

        void* instance = implementation.create(child);
        child->self.reset(
            instance,
            implementation.getAPI(instance),
            implementation.destroy
        );

        if (implementation.configure)
            implementation.configure(instance);

        Logger::info(moduleName, "> use '{}' = '{}'", typeName<API>(), implName);
        return Slot<API>{&child->self};
    }

    template<typename API>
    Slot<API> get(std::string_view instanceName = "default") {
        noteRequire<API>();
        if (auto local = getLocal<API>(instanceName); local.exists())
            return local;
        if (parent)
            return parent->get<API>(instanceName);
        return {};
    }

    template<typename T>
    T* require(std::string_view instanceName = "default") {
        noteRequire<T>();
        Slot<T> slot = get<T>(instanceName);
        if (!slot.exists()) {
            throw std::runtime_error(std::format(
                "Component '{}' with instance '{}' not found",
                typeName<T>(), instanceName));
        }
        return slot.get();
    }

    template<typename API>
    void remove(std::string_view instanceName = "default") {
        ComponentKey key{std::string(typeName<API>()), std::string(instanceName)};
        auto it = lookup.find(key);
        if (it == lookup.end())
            return;
        Components* node = nodeOf(it->second);
        lookup.erase(it);
        auto child = std::find_if(children.begin(), children.end(),
            [node](const std::unique_ptr<Components>& p) { return p.get() == node; });
        if (child != children.end())
            children.erase(child);
    }

    void removeBranch(std::string_view instanceName) {
        remove<Components>(instanceName);
    }
};

inline Components::~Components() {
    stopServices();
    self.reset(nullptr, nullptr, nullptr);
    while (!children.empty())
        children.pop_back();
}

} // namespace Lattice
