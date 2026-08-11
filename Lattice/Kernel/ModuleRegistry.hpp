#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <stdexcept>
#include <format>

namespace Kernel {
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

    template<typename T>
    void registerAPI(T* module) {
        static_assert(requires { T::apiName; },
        "API must define static constexpr apiName");

        const std::string name{T::apiName};

        auto [it, inserted] = modules.emplace(name, static_cast<void*>(module));
        if (!inserted) {
            throw std::runtime_error(std::format("API '{}' already registered", name));
        }
    }

    template<typename T>
    T* get() {
        static_assert(requires { T::apiName; },
        "API must define static constexpr apiName");

        auto it = modules.find(std::string(T::apiName));
        if (it == modules.end())
            return nullptr;

        return static_cast<T*>(it->second);
    }

    bool contains(std::string_view name) const {
        return modules.contains(std::string(name));
    }

    template<typename T>
    bool contains() const {
        return contains(T::apiName);
    }

private:
    std::unordered_map<std::string, void*> modules;
};
}