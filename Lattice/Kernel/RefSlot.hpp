#pragma once

#include <cstdint>

namespace Lattice {

class Node;

template<typename T>
struct Slot {
    Node* node = nullptr;

    Slot() = default;
    Slot(Node* node) : node(node) {}

    T* get() const noexcept;
    T* operator->() const noexcept { return get(); }
    T& operator*() const noexcept { return *get(); }

    bool exists() const noexcept;

    explicit operator bool() const noexcept {
        return exists();
    }

    bool operator==(std::nullptr_t) const noexcept {
        return !exists();
    }

    bool operator!=(std::nullptr_t) const noexcept {
        return exists();
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