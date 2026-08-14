#pragma once
#include <string_view>
#include "Lattice/Kernel/ApiSlots.hpp"
#include <Lattice/Kernel/UniverseModelAPI.hpp>

namespace Lattice {

class Universe {
public:
    Universe(ModuleRegistry& registry);

    void configure(std::string_view modelName);
    void update();

    // Удобные обёртки
    template<typename API>
    bool use(std::string_view id) {
        return static_cast<bool>(apis.use<API>(id));
    }

    template<typename API, typename Impl>
    bool use() {
        return static_cast<bool>(apis.use<API, Impl>());
    }

    // === Главное ===
    template<typename API>
    Slot<API> require() {
        return apis.require<API>();
    }

    template<typename API>
    Slot<API> get() {
        return apis.get<API>();
    }

private:
    static constexpr std::string_view moduleName = "Universe";

    ApiSlots apis;
    Slot<UniverseModelAPI> model;
};
}