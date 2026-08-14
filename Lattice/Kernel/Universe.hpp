#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include "Lattice/Kernel/ApiSlots.hpp"
#include <Lattice/Kernel/UniverseModelAPI.hpp>

namespace Lattice {
class Universe {
public:
    Universe(ModuleRegistry& registry);

    void configure(std::string_view modelName);

    // === Управление состоянием симуляции ===
    void update();

    template<typename API>
    bool use(std::string_view id) {
        return apis.use<API>(id);
    }

    template<typename API, typename Impl>
    bool use() {
        return apis.use<API, Impl>();
    }

    template<typename API>
    API* require() {
        return apis.require<API>();
    }

    template<typename API>
    API* get() {
        return apis.get<API>();
    }

private:
    static constexpr std::string_view moduleName = "Universe";
    UniverseModelAPI* model = nullptr;
    ApiSlots apis;
};
}