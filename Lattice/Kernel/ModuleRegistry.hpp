#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>
#include <format>
#include <vector>

#include <Lattice/Kernel/TypeName.hpp>
#include <Lattice/Tools/Logger.hpp>

namespace Lattice {

class Components;
class ModuleRegistry {
public:
    using CreateFn  = void* (*)(void*);
    using GetAPIFn  = void* (*)(void*);
    using DestroyFn = void (*)(void*);

    struct ImplementationEntry {
        std::string id;
        CreateFn create = nullptr;
        GetAPIFn getAPI = nullptr;
        DestroyFn destroy = nullptr;
    };

    struct ModuleEntry {
        std::unordered_map<std::string, ImplementationEntry> implementations;
    };

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
        auto [it, inserted] = modules.emplace(typeName<API>(), ModuleEntry{});
        if (!inserted)
            throw std::runtime_error(std::format("API '{}' already registered", typeName<API>()));
        Logger::info("Registry", "+ api  {}", typeName<API>());
    }

    template<typename API, typename Impl>
    void registerImpl() {
        auto& module = modules[std::string(typeName<API>())];
        ImplementationEntry impl;
        impl.id = typeName<Impl>();
        impl.create = [](void* context) -> void* {
            // Предпочитаем конструктор от Components&, если он есть
            if constexpr (requires { Impl(std::declval<Lattice::Components&>()); }) {
                return new Impl(*static_cast<Lattice::Components*>(context));
            }
            else if constexpr (std::is_default_constructible_v<Impl>) {
                return new Impl();
            }
            else {
                static_assert(
                    requires { Impl(std::declval<Lattice::Components&>()); } ||
                    std::is_default_constructible_v<Impl>,
                    "Impl must be constructible from Components& or default-constructible"
                );
                return nullptr;
            }
        };
        impl.getAPI = [](void* instance) -> void* { return static_cast<API*>(static_cast<Impl*>(instance));};
        impl.destroy = [](void* ptr) { delete static_cast<Impl*>(ptr); };
        auto [it, inserted] = module.implementations.emplace(impl.id, impl);
        if (!inserted)
            throw std::runtime_error(std::format("Implementation '{}' already registered", typeName<API>()));
        Logger::info("Registry", "+ impl {}", typeName<Impl>());
    }

    template<typename API>
    API* get() {
        static_assert(requires { typeName<API>(); },
        "API must define static constexpr id");

        auto it = modules.find(typeName<API>());
        if (it == modules.end())
            return nullptr;

        return static_cast<API*>(it->second);
    }

    bool contains(std::string_view name) const {
        return modules.contains(std::string(name));
    }

    template<typename API>
    bool contains() const {
        return contains(typeName<API>());
    }

    template<typename API>
    ModuleEntry& requireApi() {
        auto moduleIt = modules.find(std::string(typeName<API>()));
        if (moduleIt == modules.end()) {
            throw std::runtime_error(std::format("API '{}' not registered", typeName<API>()));
        }
        return moduleIt->second;
    }

    template<typename API, typename Impl>
    ImplementationEntry& requireImpl() {
        return requireImpl<API>(typeName<API>());
    }

    template<typename API>
    ImplementationEntry& requireImpl(std::string_view id) {
        auto& module = requireApi<API>();
        auto implIt = module.implementations.find(std::string(id));
        if (implIt == module.implementations.end()) {
            throw std::runtime_error(std::format(
                "Implementation '{}' not found for API '{}'", id, typeName<API>()));
        }
        return implIt->second;
    }

private:
    std::unordered_map<std::string, ModuleEntry> modules;
};
}