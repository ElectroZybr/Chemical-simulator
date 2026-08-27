#pragma once

#include <string>

namespace Lattice {

class Components;

struct ComponentData {
    Components* owner = nullptr;
    std::string type;
    std::string apiType;
    void* instance = nullptr;
    void* api = nullptr;
    void (*destroy)(void*) = nullptr;
    void (*configure)(void*, Components&) = nullptr;

    ~ComponentData() {
        if (instance && destroy)
            destroy(instance);
    }

    void reset(void* newInstance, void* newT, void (*newDestroy)(void*),
               void (*newConfigure)(void*, Components&) = nullptr) {
        if (instance && destroy)
            destroy(instance);

        instance = newInstance;
        api = newT;
        destroy = newDestroy;
        configure = newConfigure;
    }
};

template<typename T>
struct Slot {
    ComponentData* data = nullptr;

    Slot() = default;
    explicit Slot(ComponentData* d) : data(d) {}

    T* operator->() const {
        return data ? static_cast<T*>(data->api) : nullptr;
    }

    T& operator*() const {
        return *static_cast<T*>(data->api);
    }

    explicit operator bool() const {
        return data && data->api;
    }

    T* get() const {
        return data ? static_cast<T*>(data->api) : nullptr;
    }

    bool exists() const {
        return data != nullptr;
    }

    bool ready() const {
        return data && data->api;
    }
};

template<typename T>
struct Ref {
    T* ptr = nullptr;

    Ref() = default;
    Ref(T* ptr) : ptr(ptr) {}

    T* operator->() const noexcept { return ptr; }
    T& operator*() const noexcept { return *ptr; }
    T& get() const noexcept { return *ptr; }
    bool exists() const noexcept { return ptr != nullptr; }

    explicit operator bool() const noexcept { return ptr != nullptr; }

    bool operator==(std::nullptr_t) const noexcept { return ptr == nullptr; }
    bool operator!=(std::nullptr_t) const noexcept { return ptr != nullptr; }
};
}

using Lattice::Ref;
using Lattice::Slot;