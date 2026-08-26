#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <vector>
#include <utility>

#include <Lattice/Kernel/Registry.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Requirements.hpp>
#include <Lattice/Kernel/Exception.hpp>
#include <Lattice/Kernel/RefSlot.hpp>
#include <Lattice/Tools/LogStyle.hpp>
#include <Lattice/Tools/Logger.hpp>

namespace Lattice {

class Components;
class Registry;

class Components {
    static constexpr std::string_view tag = "Components";
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

    Components* makeChild(std::string_view instanceName, std::string_view type) {
        auto node = std::make_unique<Components>(registry, this, instanceName);
        Components* raw = node.get();
        raw->self.owner = raw;
        raw->self.type = type;
        children.push_back(std::move(node));
        return raw;
    }

    std::string label() const {
        return name.empty() ? "Root" : name;
    }

    void appendTree(Logger::Tree& tree, size_t depth, const Components* highlighted) const {
        for (const auto& child : children) {
            const bool selected = child.get() == highlighted;

            std::string label = child->self.type;
            if (selected)
                label = std::format("{}{}{} 🡸{}", Color::red, Color::bold, label, Color::reset);

            tree.node(label, depth);
            child->appendTree(tree, depth + 1, highlighted);
        }
    }

    const Components* findByName(std::string_view target) const {
        if (self.type == target)
            return this;

        for (const auto& child : children) {
            if (const Components* found = child->findByName(target))
                return found;
        }

        return nullptr;
    }

    void clearChildren() {
        children.clear();
    }

    template<typename T>
    void collectInto(std::vector<T*>& out) const {
        if (self.apiType == typeName<T>() && self.api)
            out.push_back(static_cast<T*>(self.api));

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
    void addImpls() {
        for (const auto& implName : registry->implementationsOf<T>())
            add<T>(implName, implName);
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
    void add(std::string_view implName, std::string_view instanceName) {
        noteAdd<T>();

        const auto& entry = registry->requireImpl<T>(implName);
        Components* child = makeChild(instanceName, implName);

        // index(std::string(typeName<T>()), instanceName, &child->self);
        index(std::string(implName), instanceName, &child->self);
        child->self.apiType = std::string(typeName<T>());
        void* instance = entry.create(child);
        child->self.reset(
            instance,
            entry.getAPI(instance),
            entry.destroy,
            entry.configure
        );

        Logger::info(tag, "+ {} '{}' ({})", typeName<T>(), instanceName, implName);
    }

    template<typename T, typename Impl>
    void add(std::string_view instanceName = "default") {
        noteAdd<T>();

        const auto& entry = registry->requireImpl<T>(typeName<Impl>());
        Components* child = makeChild(instanceName, typeName<Impl>());

        // index(std::string(typeName<T>()), instanceName, &child->self);
        index(std::string(typeName<Impl>()), instanceName, &child->self);
        child->self.apiType = std::string(typeName<T>());
        void* instance = entry.create(child);
        child->self.reset(
            instance,
            entry.getAPI(instance),
            entry.destroy,
            entry.configure
        );

        Logger::info(tag, "+ {} '{}' ({})", typeName<T>(), instanceName, typeName<Impl>());
    }

    template<typename T>
    void add(std::string_view instanceName = "default") {
        noteAdd<T>();

        if (getLocal<T>(instanceName).exists())
            return;

        const Registry::TypeEntry* entry = registry->find(std::string(typeName<T>()));
        if (!entry || !entry->create)
            throw Lattice::Exception(tag, "Component '{}' not found", typeName<T>());

        Components* child = makeChild(instanceName, typeName<T>());
        child->self.apiType = std::string(typeName<T>());
        void* instance = entry->create(child);
        child->self.reset(
            instance,
            instance,
            entry->destroy,
            entry->configure
        );

        index(std::string(typeName<T>()), instanceName, &child->self);
        // child->index(std::string(typeName<T>()), instanceName, &child->self);

        Logger::info(tag, "+ {}", typeName<T>());
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
            throw Lattice::Exception(tag, "unknown implementation '{}' for '{}'", implName, typeName<API>());
        }

        const auto& implementation = registry->requireImpl<API>(implName);
        Components* child = nullptr;

        if (auto existing = getLocal<API>(instanceName); existing.exists()) {
            child = nodeOf(existing.data);
            if (child->self.api) {
                if constexpr (std::is_same_v<API, ServiceAPI>)
                    static_cast<ServiceAPI*>(child->self.api)->stop();
                child->self.reset(nullptr, nullptr, nullptr, nullptr);
            }
            child->children.clear();;
        } else {
            child = makeChild(instanceName, implName);
            child->self.apiType = std::string(typeName<API>());
            // интексируем <API>("name") -> Impl; <Impl>("name") -> Impl;
            index(std::string(typeName<API>()), instanceName, &child->self);
            index(std::string(implName), instanceName, &child->self);
            Logger::info(tag, "+ interface '{}'", typeName<API>());
        }

        void* instance = implementation.create(child);
        child->self.reset(
            instance,
            implementation.getAPI(instance),
            implementation.destroy,
            implementation.configure
        );

        Logger::info(tag, "> use '{}' = '{}'", typeName<API>(), implName);
        return Slot<API>{&child->self};
    }

    // ищет компонент <T> в текущем узле и родительских
    // если компонент не найден возвращает nullptr
    template<typename API>
    Slot<API> find(std::string_view instanceName = "default") {
        noteRequire<API>();
        if (auto local = getLocal<API>(instanceName); local.exists())
            return local;

        // Для runtime-конфигурации имя реализации передаётся строкой. Если
        // интерфейс с таким instance id не найден, разрешаем реализацию как
        // <Implementation>/default: ServiceAPI("ClassicMD") -> ClassicMD/default.
        if (registry->hasImpl<API>(instanceName)) {
            const ComponentKey implementationKey{
                std::string(instanceName), "default"};
            if (auto it = lookup.find(implementationKey);
                it != lookup.end() && it->second && it->second->api)
            {
                return Slot<API>{it->second};
            }
        }

        if (parent) {
            if (auto found = parent->find<API>(instanceName); found.exists())
                    return found;
            }
        // if (parent)
        //     return parent->find<API>(instanceName);
        return {};
    }

    // ищет компонент <T> в текущем узле и родительских
    // если компонент не найден кидает исключение
    template<typename T>
    Ref<T> require(std::string_view instanceName = "default") {
        noteRequire<T>();
        if (auto slot = find<T>(instanceName); slot.exists())
            return slot.get();

        // auto* root = rootNode();
        // if (root != this) {
        //     if (auto slot = root->getLocal<T>(instanceName); slot.exists())
        //         return slot.get();
        // }

        throw Lattice::Exception(tag, "Component '{}' with instance '{}' not found", typeName<T>(), instanceName);
    }

    // вызывает метод configure() у всех компонентов ветки
    void configureAll() {
        if (self.configure) {
            Logger::info(tag, "Configuring '{}'", self.type);
            self.configure(self.instance, *this);
        } else if (self.instance) {
            Logger::warning(tag, "Component '{}' has no configure callback", self.type);
        }

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

    void dumpTree(std::string_view componentName = "Unknown") const {
        const Components* highlighted = componentName.empty() ? nullptr : findByName(componentName);
        Logger::Tree tree(label());
        appendTree(tree, 0, highlighted);
        tree.print();
    }
};
} // namespace Lattice
