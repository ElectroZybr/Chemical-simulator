#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

class AtomStorage;
class SpatialGrid;

class AtomSort {
public:
    void mortonOrder(AtomStorage& atoms, const SpatialGrid& grid);
    [[nodiscard]] const std::vector<uint32_t>& oldToNew() const noexcept { return oldToNew_; }

private:
    struct AtomView;

    void updateViewFromStorage(AtomStorage& atoms, size_t index, AtomView& view);
    void applyViewToStorage(AtomStorage& atoms, size_t index, const AtomView& view);
    void reorder(AtomStorage& atoms, std::span<uint32_t> indices);
    void radixSort(std::vector<std::pair<uint64_t, uint32_t>>& data, std::vector<std::pair<uint64_t, uint32_t>>& buf, uint64_t maxKey);

    std::vector<std::pair<uint64_t, uint32_t>> keyed_;
    std::vector<std::pair<uint64_t, uint32_t>> buf_;
    std::vector<uint32_t> indices_;
    std::vector<uint32_t> oldToNew_;
};
