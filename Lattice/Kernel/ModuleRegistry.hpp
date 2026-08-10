#pragma once

#include <typeindex>
#include <unordered_map>
#include <format>

namespace Kernel {
class ModuleRegistry {
public:
    ModuleRegistry() = default;
    ModuleRegistry(const ModuleRegistry& source, const std::vector<std::string>& allowed) {
        for (const std::string id : allowed) {
            auto typeIt = source.typeMap.find(id);
            if (typeIt == source.typeMap.end())
                continue;
            auto it = source.modules.find(typeIt->second);
            if (it != source.modules.end()) {
                modules.emplace(*it);
                typeMap.emplace(*typeIt);
            }
        }
    }

    template<typename T>
    void registerAPI(T* module) {
        static_assert(requires { T::apiName; },
        "API must define static constexpr moduleName");

        const std::type_index type = typeid(T);
        auto [it, inserted] = modules.emplace(type, static_cast<void*>(module));

        if (!inserted) {
            throw std::runtime_error(std::format("API '{}' already registered", T::apiName));
        }
        typeMap.emplace(T::apiName, type);
    }

    template<typename T>
    T* get() {
        auto it = modules.find(typeid(T));

        if (it == modules.end())
            return nullptr;

        return static_cast<T*>(it->second);
    }

    std::type_index getType(std::string_view id) const {
        auto it = typeMap.find(std::string(id));

        if (it == typeMap.end()) {
            throw std::runtime_error(std::format("Unknown API '{}'", id));
        }

        return it->second;
    }

    template<typename T>
    bool contains() const {
        return modules.contains(typeid(T));
    }

private:
    std::unordered_map<std::type_index, void*> modules;
    std::unordered_map<std::string, std::type_index> typeMap;
};
}