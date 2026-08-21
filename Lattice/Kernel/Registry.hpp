#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <format>

#include <Lattice/Kernel/TypeName.hpp>
#include <Lattice/Tools/Logger.hpp>

namespace Lattice {

class Components;

class Registry {
public:
    using CreateFn  = void* (*)(Components*);
    using DestroyFn = void  (*)(void*);
    using GetAPIFn  = void* (*)(void*);

    struct TypeEntry {
        std::string name;
        CreateFn    create  = nullptr;
        DestroyFn   destroy = nullptr;
        GetAPIFn    getAPI  = nullptr;
        std::string implements; // имя API, который реализует (пустое = обычный компонент)
    };

    // -------------------------------------------------------------------------
    // Регистрация обычного компонента
    // -------------------------------------------------------------------------
    template<typename T>
    void registerComponent() {
        std::string name = std::string(typeName<T>());
        TypeEntry entry;
        entry.name = name;

        entry.create = [](Components* ctx) -> void* {
            if constexpr (std::is_constructible_v<T, Components&>) {
                return new T(*ctx);
            } else if constexpr (std::is_default_constructible_v<T>) {
                return new T();
            } else {
                static_assert(std::is_constructible_v<T, Components&> ||
                              std::is_default_constructible_v<T>,
                              "Type must be constructible from Components& or default");
                return nullptr;
            }
        };

        entry.destroy = [](void* p) { delete static_cast<T*>(p); };

        auto [it, inserted] = types.emplace(entry.name, std::move(entry));
        if (!inserted)
            throw std::runtime_error(std::format("Type '{}' already registered", name));

        Logger::info("Registry", "+ cmpt {}", name);
    }

    // -------------------------------------------------------------------------
    // Регистрация реализации интерфейса
    // -------------------------------------------------------------------------
    template<typename API, typename Impl>
    void registerImpl() {
        std::string name       = std::string(typeName<Impl>());
        std::string implements = std::string(typeName<API>());
        TypeEntry entry;
        entry.name       = name;
        entry.implements = implements;
        entry.create = [](Components* ctx) -> void* {
            if constexpr (std::is_constructible_v<Impl, Components&>) {
                return new Impl(*ctx);
            } else if constexpr (std::is_default_constructible_v<Impl>) {
                return new Impl();
            } else {
                static_assert(false, "Impl must be constructible");
                return nullptr;
            }
        };

        entry.destroy = [](void* p) { delete static_cast<Impl*>(p); };
        entry.getAPI  = [](void* p) -> void* {
            return static_cast<API*>(static_cast<Impl*>(p));
        };

        auto [it, inserted] = types.emplace(entry.name, std::move(entry));
        if (!inserted)
            throw std::runtime_error(std::format("Implementation '{}' already registered", name));

        // Запоминаем, что этот тип реализует API
        apiToImpls[implements].push_back(name);

        Logger::info("Registry", "+ impl {} -> {}", name, implements);
    }

    template<typename API>
    void registerAPI() {
        // просто запоминаем имя API, без create/destroy
        apiToImpls.try_emplace(std::string(typeName<API>()));
        Logger::info("Registry", "+ api  {}", typeName<API>());
    }

    // -------------------------------------------------------------------------
    // Запросы
    // -------------------------------------------------------------------------
    bool has(std::string_view name) const {
        std::string key(name);
        return types.contains(key) || apiToImpls.contains(key);
    }

    template<typename T>
    bool has() const {
        return has(typeName<T>());
    }

    const TypeEntry* find(std::string_view name) const {
        auto it = types.find(std::string(name));
        return it != types.end() ? &it->second : nullptr;
    }

    // Все реализации конкретного API
    const std::vector<std::string>& implementationsOf(std::string_view apiName) const {
        static const std::vector<std::string> empty;
        auto it = apiToImpls.find(std::string(apiName));
        return it != apiToImpls.end() ? it->second : empty;
    }

    template<typename API>
    const std::vector<std::string>& implementationsOf() const {
        return implementationsOf(typeName<API>());
    }

    bool hasImpl(std::string_view apiName, std::string_view implName) const {
        const auto& list = implementationsOf(apiName);
        return std::find(list.begin(), list.end(), implName) != list.end();
    }

    template<typename API>
    bool hasImpl(std::string_view implName) const {
        return hasImpl(typeName<API>(), implName);
    }

    template<typename API, typename Impl>
    bool hasImpl() const {
        return hasImpl(typeName<API>(), typeName<Impl>());
    }

    template<typename T>
    TypeEntry* get() {
        static_assert(requires { typeName<T>(); },
        "Type must define static constexpr id");
        auto it = types.find(typeName<T>());
        if (it == types.end())
            return nullptr;
        return it->second;
    }

    // Возвращает список всех зарегистрированных имён типов и API
    std::vector<std::string> listProvided() const {
        std::vector<std::string> out;
        out.reserve(types.size() + apiToImpls.size());
        for (const auto& [name, entry] : types) {
            out.push_back(name);
        }
        for (const auto& [api, impls] : apiToImpls) {
            out.push_back(api);
        }
        return out;
    }

    template<typename T>
    TypeEntry& require() {
        auto it = types.find(std::string(typeName<T>()));
        if (it == types.end()) {
            throw std::runtime_error(std::format("Type '{}' not registered", typeName<T>()));
        }
        return it->second;
    }

    template<typename API, typename Impl>
    TypeEntry& requireImpl() {
        return requireImpl<API>(typeName<Impl>());
    }

    template<typename API>
    const TypeEntry& requireImpl(std::string_view id) const {
        if (!hasImpl<API>(id)) {
            throw std::runtime_error(std::format("Implementation '{}' not found for API '{}'", id, typeName<API>()));
        }
        return types.at(std::string(id));
    }

private:
    std::unordered_map<std::string, TypeEntry> types;
    std::unordered_map<std::string, std::vector<std::string>> apiToImpls;
};

} // namespace Lattice