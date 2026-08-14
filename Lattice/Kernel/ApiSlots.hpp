#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>

#include "Lattice/Kernel/ModuleRegistry.hpp"

namespace Lattice {
class ApiSlots {
private:
    struct IHolder {
        virtual ~IHolder() = default;
    };

    template<typename API>
    struct Holder final : IHolder {
        void* instance;
        API* api;
        void (*destroy)(void*);

        Holder()
            : instance(nullptr),
            api(nullptr),
            destroy(nullptr)
        {}

        Holder(void* instance, API* api, void (*destroy)(void*))
            : instance(instance),
              api(api),
              destroy(destroy)
        {}

        ~Holder() override {
            if (instance && destroy)
                destroy(instance);
        }
    };

    ModuleRegistry* registry = nullptr;
    std::unordered_map<std::string, std::unique_ptr<IHolder>> slots;

public:
    explicit ApiSlots(ModuleRegistry* registry)
        : registry(registry) {}

    
    template<typename API>
    API* require() {
        if (!registry->contains<API>()) {
            throw std::runtime_error(std::format("API '{}' not contains", std::string(API::apiName)));
        }

        auto holder = std::make_unique<Holder<API>>();
        slots[std::string(API::apiName)] = std::move(holder);
        return get<API>();
    }

    template<typename API>
    bool use(std::string_view id) {
        auto instance = registry->create<API>(id);

        if (!instance.instance)
            return false;

        auto holder = std::make_unique<Holder<API>>(
            instance.instance,
            static_cast<API*>(instance.api),
            instance.destroy
        );

        instance.release();

        slots[std::string(API::apiName)] = std::move(holder);

        return true;
    }

    template<typename API, typename Impl>
    bool use() {
        return use<API>(Impl::apiName);
    }

    template<typename API>
    API* get() {
        auto it = slots.find(std::string(API::apiName));
        if (it == slots.end())
            return nullptr;

        auto* holder =static_cast<Holder<API>*>(it->second.get());
        return holder->api;
    }
};
}