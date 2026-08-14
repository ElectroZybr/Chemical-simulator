#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include "Lattice/Kernel/ModuleRegistry.hpp"

namespace Lattice {
// Владеющие данные (живут в map)
struct SlotData {
    void* instance = nullptr;
    void* api      = nullptr;
    void (*destroy)(void*) = nullptr;

    ~SlotData() {
        if (instance && destroy)
            destroy(instance);
    }

    void reset(void* newInstance, void* newApi, void (*newDestroy)(void*)) {
        if (instance && destroy)
            destroy(instance);
        instance = newInstance;
        api      = newApi;
        destroy  = newDestroy;
    }
};

// Лёгкий хэндл (можно копировать, хранить по значению)
template<typename API>
struct Slot {
    SlotData* data = nullptr;

    Slot() = default;
    explicit Slot(SlotData* d) : data(d) {}

    API* operator->() const {
        return data ? static_cast<API*>(data->api) : nullptr;
    }

    API& operator*() const {
        return *static_cast<API*>(data->api);
    }

    explicit operator bool() const {
        return data && data->api;
    }

    API* get() const {
        return data ? static_cast<API*>(data->api) : nullptr;
    }
};

class ApiSlots {
    ModuleRegistry* registry = nullptr;
    std::unordered_map<std::string, std::unique_ptr<SlotData>> slots;

public:
    explicit ApiSlots(ModuleRegistry* registry) : registry(registry) {}

    template<typename API>
    Slot<API> require() {
        if (!registry->contains<API>())
            throw std::runtime_error("API not registered");

        auto& slot = slots[std::string(API::apiName)];
        if (!slot)
            slot = std::make_unique<SlotData>();

        return Slot<API>{slot.get()};
    }

    template<typename API>
    Slot<API> get() {
        auto it = slots.find(std::string(API::apiName));
        if (it == slots.end() || !it->second)
            return {};
        return Slot<API>{it->second.get()};
    }

    template<typename API>
    Slot<API> use(std::string_view id) {
        auto instance = registry->create<API>(id);
        auto slot = require<API>();
        slot.data->reset(instance.instance, instance.api, instance.destroy);
        instance.release();
        return slot;
    }

    template<typename API, typename Impl>
    Slot<API> use() {
        return use<API>(Impl::id);
    }
};
}