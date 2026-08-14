#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>
#include <format>

namespace Lattice {
struct ApiInstance {
    void* instance = nullptr;
    void* api = nullptr;
    void (*destroy)(void*) = nullptr;

    ApiInstance() = default;

    ApiInstance(
        void* instance,
        void* api,
        void (*destroy)(void*)
    )
        : instance(instance)
        , api(api)
        , destroy(destroy)
    {}

    ~ApiInstance() {
        if (instance && destroy)
            destroy(instance);
    }

    ApiInstance(const ApiInstance&) = delete;
    ApiInstance& operator=(const ApiInstance&) = delete;

    ApiInstance(ApiInstance&& other) noexcept
        : instance(other.instance)
        , api(other.api)
        , destroy(other.destroy)
    {
        other.instance = nullptr;
        other.api = nullptr;
        other.destroy = nullptr;
    }

    ApiInstance& operator=(ApiInstance&& other) noexcept {
        if (this != &other) {
            if (instance && destroy)
                destroy(instance);

            instance = other.instance;
            api = other.api;
            destroy = other.destroy;

            other.instance = nullptr;
            other.api = nullptr;
            other.destroy = nullptr;
        }

        return *this;
    }

    void release() noexcept {
        instance = nullptr;
        api = nullptr;
        destroy = nullptr;
    }
};

struct ImplementationEntry {
    std::string id;
    void* (*create)() = nullptr;
    void* (*getAPI)(void*) = nullptr;
    void (*destroy)(void*) = nullptr;
};

struct ModuleEntry {
    std::unordered_map<std::string, ImplementationEntry> implementations;
};

class ModuleRegistry {
public:
    ModuleRegistry() = default;
    // Конструктор для создания ограниченного (shared) реестра
    ModuleRegistry(const ModuleRegistry& source, const std::vector<std::string>& allowed) {
        for (const std::string& id : allowed) {
            auto it = source.modules.find(id);
            if (it != source.modules.end()) {
                modules.emplace(id, it->second);
            }
        }
    }

    template<typename API>
    void registerAPI() {
        static_assert(requires { API::apiName; },
        "API must define static constexpr apiName");
        auto [it, inserted] = modules.emplace(std::string(API::apiName), ModuleEntry{});
        if (!inserted)
            throw std::runtime_error(std::format("API '{}' already registered", std::string(API::apiName)));
    }

    template<typename API, typename Impl>
    void registerImpl() {
        static_assert(requires { API::apiName; });
        static_assert(requires { Impl::id; });
        auto& module = modules[std::string(API::apiName)];
        ImplementationEntry impl;
        impl.id = std::string(Impl::id);
        impl.create = []() -> void* { return new Impl(); };
        impl.getAPI = [](void* instance) -> void* { return static_cast<API*>(static_cast<Impl*>(instance));};
        impl.destroy = [](void* ptr) { delete static_cast<Impl*>(ptr); };
        auto [it, inserted] = module.implementations.emplace(impl.id, impl);
        if (!inserted)
            throw std::runtime_error(std::format("Implementation '{}' already registered", std::string(API::apiName)));
    }

    template<typename API, typename Impl>
    ApiInstance create() const {
        return create<API>(Impl::apiName);
    }

    template<typename API>
    ApiInstance create(std::string_view id) const {
        auto moduleIt = modules.find(std::string(API::apiName));
        if (moduleIt == modules.end())
            throw std::runtime_error("API not found");

        auto& module = moduleIt->second;
        auto implIt = module.implementations.find(std::string(id));
        if (implIt == module.implementations.end()) {
            throw std::runtime_error(std::format("Implementation '{}' not found for API '{}'", id, std::string(API::apiName)));
        }

        const auto& implementation = implIt->second;
        void* instance = implementation.create();
        return {
            instance,
            implementation.getAPI(instance),
            implementation.destroy
        };
    }

    template<typename API>
    API* get() {
        static_assert(requires { API::apiName; },
        "API must define static constexpr apiName");

        auto it = modules.find(std::string(API::apiName));
        if (it == modules.end())
            return nullptr;

        return static_cast<API*>(it->second);
    }

    bool contains(std::string_view name) const {
        return modules.contains(std::string(name));
    }

    template<typename T>
    bool contains() const {
        return contains(T::apiName);
    }

private:
    std::unordered_map<std::string, ModuleEntry> modules;
};
}