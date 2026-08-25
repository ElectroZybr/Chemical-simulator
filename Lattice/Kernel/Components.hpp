#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <vector>

#include <Lattice/Tools/Logger.hpp>
#include <Lattice/Kernel/Registry.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Requirements.hpp>

namespace Lattice {

class Components;
class Registry;

struct ComponentData {
    Components* owner = nullptr;
    void* instance = nullptr;
    void* api = nullptr;
    void (*destroy)(void*) = nullptr;
    void (*configure)(void*, Components&) = nullptr;

    ~ComponentData() {
        if (instance && destroy)
            destroy(instance);
    }

    void reset(void* newInstance, void* newT, void (*newDestroy)(void*),
               void (*newConfigure)(void*, Components&) = nullptr) {
        if (instance && destroy)
            destroy(instance);

        instance = newInstance;
        api = newT;
        destroy = newDestroy;
        configure = newConfigure;
    }
};

template<typename T>
struct Slot {
    ComponentData* data = nullptr;

    Slot() = default;
    explicit Slot(ComponentData* d) : data(d) {}

    T* operator->() const {
        return data ? static_cast<T*>(data->api) : nullptr;
    }

    T& operator*() const {
        return *static_cast<T*>(data->api);
    }

    explicit operator bool() const {
        return data && data->api;
    }

    T* get() const {
        return data ? static_cast<T*>(data->api) : nullptr;
    }

    bool exists() const {
        return data != nullptr;
    }

    bool ready() const {
        return data && data->api;
    }
};

class Components {
    static constexpr std::string_view moduleName = "Components";
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

    Components* makeChild(std::string_view instanceName) {
        auto node = std::make_unique<Components>(registry, this, instanceName);
        Components* raw = node.get();
        children.push_back(std::move(node));
        return raw;
    }

    std::string label() const {
        return name.empty() ? "Root" : name;
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

    template<typename T>
    void collectInto(std::vector<T*>& out) const {
        const std::string type{typeName<T>()};

        for (const auto& [key, data] : lookup) {
            if (key.first != type)
                continue;
            if (!data || !data->instance)
                continue;

            out.push_back(static_cast<T*>(data->instance));
        }

        for (const auto& child : children)
            child->collectInto<T>(out);
    }

    const Components* rootNode() const {
        const Components* n = this;
        while (n->parent)
            n = n->parent;
        return n;
    }

public:
    explicit Components(Registry* registry, Components* parent = nullptr, std::string_view name = "Root")
        : registry(registry), parent(parent), name(name) {
        self.owner = this;
    }

    Components(const Components&) = delete;
    Components& operator=(const Components&) = delete;
    Components(Components&&) = delete;
    Components& operator=(Components&&) = delete;

    // создает и возвращает объекты интерфейса <T> найденные в глобальном registry
    template<typename T>
    std::vector<T*> addImpls() {
        std::vector<T*> result;
        for (const auto& implName : registry->implementationsOf<T>())
            result.push_back(&add<T>(implName, implName));
        return result;
    }

    // возвращает объекты интерфейса <T> из текущего узла и его потомков
    template<typename T>
    std::vector<T*> localCollect() const {
        std::vector<T*> out;
        collectInto<T>(out);
        return out;
    }

    // возвращает все объекты интерфейса <T> существующие в дереве (начиная с root)
    template<typename T>
    std::vector<T*> globalCollect() const {
        return rootNode()->localCollect<T>();
    }

    template<typename T>
    T& add(std::string_view implName, std::string_view instanceName = "default") {
        const auto& entry = registry->requireImpl<T>(implName);
        Components* child = makeChild(instanceName);

        index(std::string(typeName<T>()), instanceName, &child->self);
        index(std::string(implName), instanceName, &child->self);

        void* instance = entry.create(child);
        child->self.reset(instance, entry.getAPI(instance), entry.destroy, entry.configure);

        Logger::info(moduleName, "+ {} '{}' ({})",
            typeName<T>(), instanceName, implName);

        return *static_cast<T*>(instance);
    }

    template<typename T, typename Impl>
    Impl& add(std::string_view instanceName = {}) {
        const auto& entry = registry->requireImpl<T>(typeName<Impl>());
        const std::string_view id = instanceName.empty()
            ? typeName<Impl>()
            : instanceName;
        Components* child = makeChild(id);

        index(std::string(typeName<T>()), id, &child->self);
        index(std::string(typeName<Impl>()), id, &child->self);

        void* instance = entry.create(child);
        child->self.reset(instance, entry.getAPI(instance), entry.destroy, entry.configure);

        Logger::info(moduleName, "+ {} '{}' ({})",
            typeName<T>(), id, typeName<Impl>());

        return *static_cast<Impl*>(instance);
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

        Components* child = makeChild(instanceName);
        void* instance = entry->create(child);
        child->self.reset(instance, instance, entry->destroy, entry->configure);
        index(std::string(typeName<T>()), instanceName, &child->self);
        child->index(std::string(typeName<T>()), instanceName, &child->self);

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
            child->children.clear();;
        } else {
            child = makeChild(instanceName);
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

        Logger::info(moduleName, "> use '{}' = '{}'", typeName<API>(), implName);
        return Slot<API>{&child->self};
    }

    // ищет компонент <T> в текущем узле и родительских
    // если компонент не найден кидает исключение
    template<typename API>
    Slot<API> get(std::string_view instanceName = "default") {
        noteRequire<API>();
        if (auto local = getLocal<API>(instanceName); local.exists())
            return local;
        if (parent)
            return parent->get<API>(instanceName);
        return {};
    }

    // ищет компонент <T> в текущем узле и родительских
    // если компонент не найден кидает исключение
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

    // вызывает метод configure() у всех компонентов ветки
    void configureAll() {
        if (self.configure)
            self.configure(self.instance, *this);

        for (auto& child : children)
            child->configureAll();
    }

    // удаляет компонент <T> из ветки
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

    // останавливает все сервисы
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

    ~Components() {
        stopServices();
        self.reset(nullptr, nullptr, nullptr);
        while (!children.empty())
            children.pop_back();
    }
};


} // namespace Lattice