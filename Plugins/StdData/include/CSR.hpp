#pragma once

#include <span>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace StdData {

template<typename Type, typename Offset = uint32_t>
class CSR {
public:
    std::span<Type> get(size_t i) noexcept {
        const size_t begin = offsets_[i];
        return {values_.data() + begin, offsets_[i + 1] - begin};
    }

    std::span<const Type> get(size_t i) const noexcept {
        const size_t begin = offsets_[i];
        return {values_.data() + begin, offsets_[i + 1] - begin};
    }

    std::span<Type> operator[](size_t i) noexcept {
        return get(i);
    }

    std::span<const Type> operator[](size_t i) const noexcept {
        return get(i);
    }

    size_t size() const noexcept {
        return offsets_.size() > 0 ? offsets_.size() - 1 : 0;
    }

    size_t valueCount() const noexcept {
        return values_.size();
    }

    std::span<uint32_t> offsets() noexcept { return offsets_; }
    std::span<uint32_t> values() noexcept { return values_; }

    void resize(size_t valueCount, size_t rowCount) {
        values_.resize(valueCount);
        offsets_.resize(rowCount);
    }

    void clear() {
        values_.clear();
        offsets_.clear();
    }

    size_t memoryBytes() const noexcept {
        return values_.capacity() * sizeof(Type) + offsets_.capacity() * sizeof(Offset);
    }

private:
    std::vector<Type> values_;    // частицы сгруппированные подряд по ячейкам
    std::vector<Offset> offsets_; // массив оффсетов (каждый оффсет - начало новой ячейки)
};
}