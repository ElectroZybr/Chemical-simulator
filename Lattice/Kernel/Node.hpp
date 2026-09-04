#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <utility>

#include <Lattice/Kernel/Registry.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Requirements.hpp>
#include <Lattice/Kernel/Exception.hpp>
#include <Lattice/Kernel/RefSlot.hpp>
#include <Lattice/Kernel/Path.hpp>
#include <Lattice/Tools/LogStyle.hpp>
#include <Lattice/Tools/Logger.hpp>


namespace Lattice {

class Registry;

class Node {
    static constexpr std::string_view tag = "Node";

    Registry& registry;
    ObjectRegistry& objectRegistry;
    Node* parent = nullptr;
    ObjectId id = 0;

    std::vector<std::unique_ptr<Node>> children;

    using DestroyFn   = void(*)(void*);
    using ConfigureFn = void(*)(void*, Node&);

    void* instance        = nullptr;
    void* api             = nullptr;
    DestroyFn destroy     = nullptr;
    ConfigureFn configure = nullptr;

    Node* makeChild(std::string_view name, std::string_view type) {
        auto node = std::make_unique<Node>(registry, objectRegistry, this);
        Node* raw = node.get();
        raw->id = objectRegistry.create(id, type, name, raw);
        children.push_back(std::move(node));
        return raw;
    }

    void appendTree(Logger::Tree& tree, size_t depth, const Node* highlighted) const {
        for (const auto& child : children) {
            const bool selected = child.get() == highlighted;

            const Entry& entry = objectRegistry.require(child->id);
            // std::string label = child->self.type;
            // if (selected)
            //     // label = Lattice::Text("<b><r>{} 🡸<//>");
            //     label = std::format("{}{}{} 🡸{}", Color::red, Color::bold, label, Color::reset);

            tree.node(std::format("{} ({}) #{}", entry.type, entry.name, child->id), depth);
            child->appendTree(tree, depth + 1, highlighted);
        }
    }

    template<typename T>
    void collectInto(std::vector<T*>& out) const {
        if (void* object = getObject();
            object && objectRegistry[id].type == typeName<T>())
        {
            out.push_back(static_cast<T*>(object));
        }

        for (const auto& child : children)
            child->collectInto<T>(out);
    }

    const Node* rootNode() const {
        const Node* n = this;
        while (n->parent)
            n = n->parent;
        return n;
    }

    Node* rootNode() {
        Node* n = this;
        while (n->parent)
            n = n->parent;
        return n;
    }

public:
    explicit Node(Registry& registry, ObjectRegistry& objectRegistry, Node* parent = nullptr)
        : registry(registry), objectRegistry(objectRegistry), parent(parent) {
            if (!parent)
                id = objectRegistry.create(InvalidObjectId, "Root", "Root", this);
    }

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    // создает и возвращает объекты интерфейса <T> найденные в глобальном registry
    template<typename T>
    void addImpls() {
        for (const auto& implName : registry.implementationsOf<T>())
            add<T>(implName, implName);
    }

    template<typename T>
    std::vector<T*> directCollect() const {
        std::vector<T*> out;

        for (const auto& child : children) {
            if (void* object = child->getObject();
                object && objectRegistry[child->id].type == typeName<T>())
            {
                out.push_back(static_cast<T*>(object));
            }
        }

        return out;
    }

    // возвращает объекты интерфейса <T> из текущего узла и его потомков
    template<typename T>
    std::vector<T*> folderCollect() const {
        std::vector<T*> out;
        collectInto<T>(out);
        return out;
    }

    // возвращает все объекты интерфейса <T> существующие в дереве (начиная с root)
    template<typename T>
    std::vector<T*> globalCollect() const {
        return rootNode()->folderCollect<T>();
    }

    Node& addFolder(std::string_view name) {
        Node* raw = makeChild(name, "Folder");
        return *raw;
    }

    template<typename T>
    void add(std::string_view implName, std::string_view instanceName) {
        noteAdd<T>();

        const auto& entry = registry.requireImpl<T>(implName);
        Node* child = makeChild(instanceName, implName);

        void* newInstance = entry.create(child);

        child->instance = newInstance;
        child->api = entry.getAPI(newInstance);
        child->destroy = entry.destroy;
        child->configure = entry.configure;

        Logger::info(tag, "+ {} '{}' ({})", typeName<T>(), instanceName, implName);
    }

    template<typename T, typename Impl>
    void add(std::string_view instanceName = "default") {
        noteAdd<T>();

        const auto& entry = registry.requireImpl<T>(typeName<Impl>());
        Node* child = makeChild(instanceName, typeName<Impl>());

        void* newInstance = entry.create(child);

        child->instance = newInstance;
        child->api = entry.getAPI(newInstance);
        child->destroy = entry.destroy;
        child->configure = entry.configure;

        Logger::info(tag, "+ {} '{}' ({})", typeName<T>(), instanceName, typeName<Impl>());
    }

    template<typename T>
    void add(std::string_view instanceName = "default") {
        noteAdd<T>();

        if (has(typeName<T>(), instanceName)) {
            Logger::warning(tag, "Component '{}' ('{}') already exists", typeName<T>(), instanceName);
            return;
        }

        const auto& entry = registry.require<T>();
        Node* child = makeChild(instanceName, typeName<T>());

        child->instance = entry.create(child);
        child->api = nullptr;
        child->destroy = entry.destroy;
        child->configure = entry.configure;

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

        if (!registry.hasImpl<API>(implName)) {
            throw Lattice::Exception(tag, "unknown implementation '{}' for '{}'", implName, typeName<API>());
        }

        const auto& entry = registry.requireImpl<API>(implName);
        Node* child = nullptr;
        ObjectId objectId = objectRegistry.find(id, typeName<API>(), instanceName);

        if (valid(objectId)) {
            auto* entry = objectRegistry.get(objectId);
            child = static_cast<Node*>(entry->object);
            if (child->api) {
                if constexpr (std::is_same_v<API, ServiceAPI>) {
                    static_cast<ServiceAPI*>(child->api)->stop();
                    child->destroy(child->instance);
                    child->instance = nullptr;
                    child->api = nullptr;
                    child->destroy = nullptr;
                    child->configure = nullptr;
                }
            }
            child->children.clear();;
        } else {
            child = makeChild(instanceName, implName);
            objectRegistry[child->id].type = std::string(typeName<API>());
            // добавляем алиас: <API>("name") -> Impl; <Impl>("name") -> Impl;
            objectRegistry.alias(child->id, id, typeName<API>(), instanceName);
            Logger::info(tag, "+ interface '{}'", typeName<API>());
        }

        void* newInstance = entry.create(child);

        child->instance = newInstance;
        child->api = entry.getAPI(newInstance);
        child->destroy = entry.destroy;
        child->configure = entry.configure;

        Logger::info(tag, "> use '{}' = '{}'", typeName<API>(), implName);
        return child;
    }

    // ищет компонент <T> в текущем узле и родительских
    // если компонент не найден возвращает nullptr
    template<typename API>
    Slot<API> find(std::string_view instanceName = "default") {
        noteRequire<API>();
        const auto apiType = typeName<API>();
        ObjectId objectId = objectRegistry.find(id, apiType, instanceName);

        if (valid(objectId)) {
            if (auto* entry = objectRegistry.get(objectId)) {
                Node* node = static_cast<Node*>(entry->object);

                if (node && node->getObject())
                    return Slot<API>(node);
            }
        }

        // Для runtime-конфигурации допускаем:
        // ServiceAPI("ClassicMD")
        //     ↓
        // ClassicMD("default")
        // То есть instanceName может фактически быть именем реализации.
        if (registry.hasImpl<API>(instanceName)) {
            ObjectId objectId = objectRegistry.find(
                id,
                instanceName,
                "default"
            );

            if (valid(objectId)) {
                if (auto* entry = objectRegistry.get(objectId)) {
                    Node* node = static_cast<Node*>(entry->object);

                    if (node && node->getObject())
                        return Slot<API>(node);
                }
            }
        }

        if (parent)
            return parent->find<API>(instanceName);

        return {};
    }

    bool has(std::string_view type, std::string_view name) const {
        return valid(objectRegistry.find(id, type, name));
    }

    // ищет компонент <T> в текущем узле и родительских
    // если компонент не найден кидает исключение
    template<typename T>
    Ref<T> require(std::string_view instanceName = "default") {
        noteRequire<T>();
        if (auto slot = find<T>(instanceName); slot.exists())
            return Ref<T>(slot.get());

        throw Lattice::Exception(tag, "Component '{}' with instance '{}' not found", typeName<T>(), instanceName);
    }

    // вызывает метод configure() у всех компонентов ветки
    void configureAll() {
        if (configure) {
            Logger::info(tag, "Configuring '{}'", objectRegistry[id].type);
            configure(instance, *this);
        } else if (instance) {
            Logger::warning(tag, "Component '{}' has no configure callback", objectRegistry[id].type);
        }

        for (auto& child : children)
            child->configureAll();
    }

    // удаляет компонент <T> из ветки
    template<typename API>
    void remove(std::string_view instanceName = "default") {
        ObjectId objectId = objectRegistry.find(id, typeName<API>(), instanceName);
        if (!valid(objectId))
            return;

        auto* entry = objectRegistry.get(objectId);
        if (!entry)
            return;

        Node* node = static_cast<Node*>(entry->object);
        if (!node)
            return;

        auto child = std::find_if(
            children.begin(),
            children.end(),
            [node](const std::unique_ptr<Node>& p) {
                return p.get() == node;
            }
        );

        if (child == children.end())
            return;

        children.erase(child);
    }

    // останавливает все сервисы
    void stopServices() {
        if (objectRegistry[id].type == typeName<ServiceAPI>() && api) {
            static_cast<ServiceAPI*>(api)->stop();
        }
        
        for (auto& child : children)
            child->stopServices();
    }

    ~Node() {
        stopServices();
        if (destroy)
            destroy(instance);
        instance = nullptr;
        api = nullptr;
        destroy = nullptr;
        configure = nullptr;
        children.clear();
        if (id)
            objectRegistry.destroy(id);
    }

    Path path() const {
        Path path;

        const Node* node = this;
        while (node && node->parent) {
            path.push(node->id);
            node = node->parent;
        }

        return path;
    }

    void dumpTree(std::string_view componentName = "Unknown") const {
        Logger::Tree tree("Root");
        appendTree(tree, 0, nullptr);
        tree.print();
    }

    void* getObject() const noexcept {
        return api ? api : instance;
    }
};

template<typename T>
T* Slot<T>::get() const noexcept {
    return node
        ? static_cast<T*>(node->getObject())
        : nullptr;
}

template<typename T>
bool Slot<T>::exists() const noexcept {
    return get() != nullptr;
}
} // namespace Lattice
