#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <Lattice/Kernel/TypeName.hpp>
#include <Lattice/Kernel/Exception.hpp>

#include "StdIo/include/Document.hpp"


namespace StdData {

class SoA {
    static constexpr std::string_view tag = "SoA";
public:
    SoA() = default;
    SoA(const SoA&) = delete;
    SoA& operator=(const SoA&) = delete;
    SoA(SoA&& other) noexcept
        : size_(std::exchange(other.size_, 0))
        , capacity_(std::exchange(other.capacity_, 0))
        , storageBytes_(std::exchange(other.storageBytes_, 0))
        , storage_(std::exchange(other.storage_, nullptr))
        , columns_(std::move(other.columns_))
    {}

    SoA& operator=(SoA&& other) noexcept {
        if (this == &other) return *this;
        releaseStorage();
        size_         = std::exchange(other.size_, 0);
        capacity_     = std::exchange(other.capacity_, 0);
        storageBytes_ = std::exchange(other.storageBytes_, 0);
        storage_      = std::exchange(other.storage_, nullptr);
        columns_      = std::move(other.columns_);
        return *this;
    }

    ~SoA() {
        releaseStorage();
    }

    // -----------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------

    void clear() noexcept { size_ = 0; }

    void reserve(size_t required) {
        if (required <= capacity_) return;

        size_t newCapacity = capacity_ == 0 ? required : capacity_;
        while (newCapacity < required)
            newCapacity = newCapacity * 3 / 2 + 1;

        relayout(newCapacity);
    }

    void resize(size_t n) {
        reserve(n);
        size_ = n;
    }

    [[nodiscard]] size_t size()         const noexcept { return size_; }
    [[nodiscard]] size_t capacity()     const noexcept { return capacity_; }
    [[nodiscard]] size_t storageBytes() const noexcept { return storageBytes_; }
    // -------
    // Add/remove column
    template<class Tag>
    typename Tag::type* addCol() {
        using T = typename Tag::type;
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(std::is_standard_layout_v<T>);

        const size_t id = typeId<Tag>();

        if (id >= columns_.size())
            columns_.resize(id + 1);

        Column& col = columns_[id];
        if (col.active) {
            assert(col.typeKey == typeToken<T>());
            return get<Tag>();
        }

        col.name = Lattice::typeName<Tag>();
        col.elementSize = sizeof(T);
        col.alignment   = alignof(T);
        col.typeKey     = typeToken<T>();
        col.active      = true;

        col.assign = [](std::byte* storage, size_t index, const Value& value) {
            auto* data = reinterpret_cast<T*>(storage);
            data[index] = value.as<T>();
        };

        relayout(capacity_);

        return get<Tag>();
    }

    template<class Tag>
    void remove() {
        const size_t id = typeId<Tag>();

        if (id >= columns_.size() || !columns_[id].active) {
            throw Lattice::Exception(tag, "Column '{}' not found", Lattice::typeName<Tag>());
        }

        columns_[id] = Column{};
        relayout(capacity_);
    }
    // -------
    // Сырой указатель на колонку
    template<class Tag>
    [[nodiscard]] typename Tag::type* get() noexcept {
        using T = typename Tag::type;
        auto* column = findColumn<Tag>();
        if (!column)
            return nullptr;
        return reinterpret_cast<T*>(storage_ + column->offset);
    }

    template<class Tag>
    [[nodiscard]] const typename Tag::type* get() const noexcept {
        using T = typename Tag::type;
        const auto* column = findColumn<Tag>();
        if (!column)
            return nullptr;
        return reinterpret_cast<T*>(storage_ + column->offset);
    }

    [[nodiscard]] void* get(std::string_view name) noexcept {
        for (auto& column : columns_) {
            if (column.active && column.name == name)
                return storage_ + column.offset;
        }

        return nullptr;
    }

    [[nodiscard]] const void* get(std::string_view name) const noexcept {
        for (const auto& column : columns_) {
            if (column.active && column.name == name)
                return storage_ + column.offset;
        }

        return nullptr;
    }

    // -------
    // require
    template<class Tag>
    [[nodiscard]] typename Tag::type* require() {
        using T = typename Tag::type;
        auto* column = findColumn<Tag>();
        if (!column) {
            throw Lattice::Exception(tag, "Column '{}' not found", Tag::name);
        }
        return reinterpret_cast<T*>(storage_ + column->offset);
    }

    template<class Tag>
    [[nodiscard]] const typename Tag::type* require() const {
        using T = typename Tag::type;
        const auto* column = findColumn<Tag>();
        if (!column) {
            throw Lattice::Exception(tag, "Column '{}' not found", Tag::name);
        }
        return reinterpret_cast<const T*>(storage_ + column->offset);
    }
    // -------
    // span
    template<class Tag>
    [[nodiscard]] std::span<typename Tag::type> span() noexcept {
        return {get<Tag>(), size_};
    }

    template<class Tag>
    [[nodiscard]] std::span<const typename Tag::type> span() const noexcept {
        return {get<Tag>(), size_};
    }
    // -------
    // доступ по индексу
    template<class Tag>
    [[nodiscard]] typename Tag::type& at(size_t index) noexcept {
        return get<Tag>()[index];
    }

    template<class Tag>
    [[nodiscard]] const typename Tag::type& at(size_t index) const noexcept {
        return get<Tag>()[index];
    }

    void set(std::string_view name, size_t index, const Value& value) {
        Column* col = findColumn(name);

        if (!col)
            throw Lattice::Exception(tag, "Column '{}' not found", name);

        if (index >= size_)
            throw Lattice::Exception(tag, "Index {} out of range for column '{}'", index, name);

        col->assign(storage_ + col->offset, index, value);
    }

    void swapRows(size_t a, size_t b) noexcept {
        if (a == b) return;

        alignas(64) std::byte scratch[64];

        for (const Column& col : columns_) {
            if (!col.active) continue;

            std::byte* lhs = storage_ + col.offset + a * col.elementSize;
            std::byte* rhs = storage_ + col.offset + b * col.elementSize;

            if (col.elementSize <= sizeof(scratch)) {
                std::memcpy(scratch, lhs, col.elementSize);
                std::memcpy(lhs, rhs, col.elementSize);
                std::memcpy(rhs, scratch, col.elementSize);
            } else {
                std::vector<std::byte> tmp(col.elementSize);
                std::memcpy(tmp.data(), lhs, col.elementSize);
                std::memcpy(lhs, rhs, col.elementSize);
                std::memcpy(rhs, tmp.data(), col.elementSize);
            }
        }
    }

    size_t size() {
        return size_;
    }

private:
    struct Column {
        std::string name;
        size_t offset       = 0;
        size_t elementSize  = 0;
        size_t alignment    = 0;
        const void* typeKey = nullptr;
        bool active         = false;
        void (*assign)(std::byte*, size_t, const Value&);
    };

    size_t size_         = 0;
    size_t capacity_     = 0;
    size_t storageBytes_ = 0;
    std::byte* storage_  = nullptr;
    std::vector<Column> columns_;

    // ----- type → dense id -----
    static size_t& nextTypeId() {
        static size_t counter = 0;
        return counter;
    }

    template<class Tag>
    static size_t typeId() {
        static const size_t id = nextTypeId()++;
        return id;
    }

    template<typename T>
    static const void* typeToken() noexcept {
        static int token;
        return &token;
    }

    static size_t alignUp(size_t value, size_t alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    template<class Tag>
    Column* findColumn() noexcept {
        const auto id = typeId<Tag>();

        if (id >= columns_.size())
            return nullptr;

        Column& col = columns_[id];

        if (!col.active)
            return nullptr;

        if (col.typeKey != typeToken<typename Tag::type>())
            return nullptr;
        return &col;
    }

    Column* findColumn(std::string_view name) noexcept {
        for (auto& col : columns_) {
            if (col.active && col.name == name)
                return &col;
        }

        return nullptr;
    }

    static std::byte* allocate(size_t bytes) {
        return bytes ? new std::byte[bytes]{} : nullptr;
    }

    static void deallocate(std::byte* ptr) noexcept {
        delete[] ptr;
    }

    void releaseStorage() noexcept {
        deallocate(storage_);
        storage_ = nullptr;
        storageBytes_ = 0;
        capacity_ = 0;
    }

    void relayout(size_t newCapacity) {
        const size_t oldColumnCount = columns_.size();
        std::vector<Column> oldColumns = columns_;
        std::byte* oldStorage = storage_;

        size_t totalBytes = 0;
        for (Column& col : columns_) {
            if (!col.active) continue;
            totalBytes = alignUp(totalBytes, col.alignment);
            col.offset = totalBytes;
            totalBytes += col.elementSize * newCapacity;
        }

        std::byte* newStorage = allocate(totalBytes);

        if (oldStorage && newStorage && size_ > 0) {
            for (size_t i = 0; i < oldColumnCount; ++i) {
                if (!oldColumns[i].active) continue;
                std::memcpy(
                    newStorage + columns_[i].offset,
                    oldStorage + oldColumns[i].offset,
                    oldColumns[i].elementSize * size_
                );
            }
        }

        deallocate(oldStorage);
        storage_ = newStorage;
        storageBytes_ = totalBytes;
        capacity_ = newCapacity;
    }
};
}