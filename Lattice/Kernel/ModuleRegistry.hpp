#pragma once

#include <typeindex>
#include <unordered_map>

namespace Kernel {
class ModuleRegistry {
public:
    template<typename T>
    void add(std::string id, T* module) {
        modules[id] = module;
    }

    template<typename T>
    T* get(const std::string& id) {
        auto it = modules.find(id);

        if (it == modules.end())
            return nullptr;

        return static_cast<T*>(it->second);
    }

    bool contains(const std::string& id) const {
        return modules.contains(id);
    }

private:
    std::unordered_map<std::string, void*> modules;
};
}