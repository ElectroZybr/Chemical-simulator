#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>

#include "Lattice/Engine/NeighborSearch/SpatialGrid.h"
#include "Lattice/Engine/DynamicSoA.h"
#include "Lattice/Engine/physics/Atom/AtomData.h"
#include "Lattice/Engine/physics/Atom/AtomSort.h"

class AtomStorage {
public:
    using AtomId = uint32_t;
    static constexpr AtomId InvalidAtomId = std::numeric_limits<AtomId>::max();

    enum class Column : uint32_t {
        X,
        Y,
        Z,
        Vx,
        Vy,
        Vz,
        Fx,
        Fy,
        Fz,
        Pfx,
        Pfy,
        Pfz,
        Energy,
        InvMass,
        Charge,
        Type,
        Valence,
        Hybridization,
        Id
    };

private:
    static constexpr size_t InvalidIndex = static_cast<size_t>(-1);

    AtomSort sort_;
    DynamicSoA buffer_;
    Column fxCol_ = Column::Fx;
    Column fyCol_ = Column::Fy;
    Column fzCol_ = Column::Fz;
    Column pfxCol_ = Column::Pfx;
    Column pfyCol_ = Column::Pfy;
    Column pfzCol_ = Column::Pfz;
    size_t mobileCount_ = 0;
    std::vector<size_t> atomIdToIndex_;
    AtomId nextAtomId_ = 0;

    static DynamicSoA makeSchema() {
        DynamicSoA schema;
        schema.add<float>(static_cast<uint32_t>(Column::X));
        schema.add<float>(static_cast<uint32_t>(Column::Y));
        schema.add<float>(static_cast<uint32_t>(Column::Z));
        schema.add<float>(static_cast<uint32_t>(Column::Vx));
        schema.add<float>(static_cast<uint32_t>(Column::Vy));
        schema.add<float>(static_cast<uint32_t>(Column::Vz));
        schema.add<float>(static_cast<uint32_t>(Column::Fx));
        schema.add<float>(static_cast<uint32_t>(Column::Fy));
        schema.add<float>(static_cast<uint32_t>(Column::Fz));
        schema.add<float>(static_cast<uint32_t>(Column::Pfx));
        schema.add<float>(static_cast<uint32_t>(Column::Pfy));
        schema.add<float>(static_cast<uint32_t>(Column::Pfz));
        schema.add<float>(static_cast<uint32_t>(Column::Energy));
        schema.add<float>(static_cast<uint32_t>(Column::InvMass));
        schema.add<float>(static_cast<uint32_t>(Column::Charge));
        schema.add<AtomData::Type>(static_cast<uint32_t>(Column::Type));
        schema.add<uint8_t>(static_cast<uint32_t>(Column::Valence));
        schema.add<uint8_t>(static_cast<uint32_t>(Column::Hybridization));
        schema.add<AtomId>(static_cast<uint32_t>(Column::Id));
        return schema;
    }

    template<typename T>
    T* col(Column c) {
        return buffer_.column<T>(static_cast<uint32_t>(c));
    }

    template<typename T>
    const T* col(Column c) const {
        return buffer_.column<T>(static_cast<uint32_t>(c));
    }

    template<typename T>
    std::span<T> colSpan(Column c) {
        return buffer_.span<T>(static_cast<uint32_t>(c));
    }

    template<typename T>
    std::span<const T> colSpan(Column c) const {
        return buffer_.span<T>(static_cast<uint32_t>(c));
    }

    template<typename T>
    T& at(Column c, size_t i) {
        return col<T>(c)[i];
    }

    template<typename T>
    const T& at(Column c, size_t i) const {
        return col<T>(c)[i];
    }

    void setVec3(Column x, Column y, Column z, size_t i, const glm::vec3& v) {
        at<float>(x, i) = v.x;
        at<float>(y, i) = v.y;
        at<float>(z, i) = v.z;
    }

    void resetForceColumns() {
        fxCol_ = Column::Fx;
        fyCol_ = Column::Fy;
        fzCol_ = Column::Fz;
        pfxCol_ = Column::Pfx;
        pfyCol_ = Column::Pfy;
        pfzCol_ = Column::Pfz;
    }

public:
    AtomStorage() : buffer_(makeSchema()) {}

    AtomStorage(const AtomStorage&) = delete;
    AtomStorage& operator=(const AtomStorage&) = delete;
    AtomStorage(AtomStorage&&) noexcept = default;
    AtomStorage& operator=(AtomStorage&&) noexcept = default;

    void clear() {
        buffer_.clear();
        resetForceColumns();
        mobileCount_ = 0;
        nextAtomId_ = 0;
        atomIdToIndex_.clear();
        atomIdToIndex_.shrink_to_fit();
    }

    void reserve(size_t count) {
        buffer_.reserve(count);
        atomIdToIndex_.reserve(count);
    }

    void resize(size_t count) { buffer_.resize(count); }

    [[nodiscard]] AtomId addAtom(const glm::vec3& coords, const glm::vec3& speed, AtomData::Type typeValue, bool fixed = false, AtomData::Hybridization hybridization = AtomData::Hybridization::None) {
        buffer_.resize(size() + 1);
        const size_t i = size() - 1;

        setPos(i, coords);
        setVel(i, speed);
        setForce(i, glm::vec3(0.0f));
        setPrevForce(i, glm::vec3(0.0f));
        at<float>(Column::Energy, i) = 0.0f;

        const auto& props = AtomData::getProps(typeValue);
        at<float>(Column::InvMass, i) = 1.0f / props.mass;
        at<float>(Column::Charge, i) = props.defaultCharge;
        at<AtomData::Type>(Column::Type, i) = typeValue;
        at<uint8_t>(Column::Valence, i) = props.maxValence;
        at<uint8_t>(Column::Hybridization, i) = (uint8_t)hybridization;

        const AtomId id = nextAtomId_++;
        setAtomId(i, id);

        if (!fixed) {
            swapAtoms(i, mobileCount_);
            ++mobileCount_;
        }
        return id;
    }

    void removeAtom(size_t index) {
        if (index >= size()) {
            return;
        }

        const size_t last = size() - 1;
        if (index < mobileCount_) {
            swapAtoms(index, mobileCount_ - 1);
            --mobileCount_;
            swapAtoms(mobileCount_, last);
        } else if (index != last) {
            swapAtoms(index, last);
        }

        atomIdToIndex_[atomId(last)] = InvalidIndex;
        buffer_.resize(last);
    }

    void swapAtoms(size_t a, size_t b) {
        if (a >= size() || b >= size() || a == b) {
            return;
        }
        buffer_.swapRows(a, b);
        atomIdToIndex_[atomId(a)] = a;
        atomIdToIndex_[atomId(b)] = b;
    }

    void swapPrevCurrentForces() {
        std::swap(fxCol_, pfxCol_);
        std::swap(fyCol_, pfyCol_);
        std::swap(fzCol_, pfzCol_);
    }

    size_t size() const { return buffer_.size(); }
    size_t mobileCount() const { return mobileCount_; }
    bool empty() const { return size() == 0; }
    bool isAtomFixed(size_t i) const { return i >= mobileCount_; }

    size_t memoryBytes() const { return buffer_.storageBytes() + atomIdToIndex_.capacity() * sizeof(size_t); }

    std::span<float> x() { return colSpan<float>(Column::X); }
    std::span<const float> x() const { return colSpan<float>(Column::X); }

    std::span<float> y() { return colSpan<float>(Column::Y); }
    std::span<const float> y() const { return colSpan<float>(Column::Y); }

    std::span<float> z() { return colSpan<float>(Column::Z); }
    std::span<const float> z() const { return colSpan<float>(Column::Z); }

    std::span<float> vx() { return colSpan<float>(Column::Vx); }
    std::span<const float> vx() const { return colSpan<float>(Column::Vx); }

    std::span<float> vy() { return colSpan<float>(Column::Vy); }
    std::span<const float> vy() const { return colSpan<float>(Column::Vy); }

    std::span<float> vz() { return colSpan<float>(Column::Vz); }
    std::span<const float> vz() const { return colSpan<float>(Column::Vz); }

    std::span<float> fx() { return colSpan<float>(fxCol_); }
    std::span<const float> fx() const { return colSpan<float>(fxCol_); }

    std::span<float> fy() { return colSpan<float>(fyCol_); }
    std::span<const float> fy() const { return colSpan<float>(fyCol_); }

    std::span<float> fz() { return colSpan<float>(fzCol_); }
    std::span<const float> fz() const { return colSpan<float>(fzCol_); }

    std::span<float> pfx() { return colSpan<float>(pfxCol_); }
    std::span<const float> pfx() const { return colSpan<float>(pfxCol_); }

    std::span<float> pfy() { return colSpan<float>(pfyCol_); }
    std::span<const float> pfy() const { return colSpan<float>(pfyCol_); }

    std::span<float> pfz() { return colSpan<float>(pfzCol_); }
    std::span<const float> pfz() const { return colSpan<float>(pfzCol_); }

    std::span<float> charge() { return colSpan<float>(Column::Charge); }
    std::span<const float> charge() const { return colSpan<float>(Column::Charge); }

    std::span<float> energy() { return colSpan<float>(Column::Energy); }
    std::span<const float> energy() const { return colSpan<float>(Column::Energy); }

    std::span<float> invMass() { return colSpan<float>(Column::InvMass); }
    std::span<const float> invMass() const { return colSpan<float>(Column::InvMass); }

    std::span<AtomData::Type> type() { return colSpan<AtomData::Type>(Column::Type); }
    std::span<const AtomData::Type> type() const { return colSpan<AtomData::Type>(Column::Type); }

    std::span<uint8_t> valence() { return colSpan<uint8_t>(Column::Valence); }
    std::span<const uint8_t> valence() const { return colSpan<uint8_t>(Column::Valence); }

    std::span<uint8_t> hybridization() { return colSpan<uint8_t>(Column::Hybridization); }
    std::span<const uint8_t> hybridization() const { return colSpan<uint8_t>(Column::Hybridization); }

    std::span<AtomId> id() { return colSpan<AtomId>(Column::Id); }
    std::span<const AtomId> id() const { return colSpan<AtomId>(Column::Id); }
    std::span<const float> floatDataSpan() const { return {}; }

    glm::vec3 pos(size_t i) const { return {x()[i], y()[i], z()[i]}; }
    glm::vec3 vel(size_t i) const { return {vx()[i], vy()[i], vz()[i]}; }
    glm::vec3 force(size_t i) const { return {fx()[i], fy()[i], fz()[i]}; }
    glm::vec3 prevForce(size_t i) const { return {pfx()[i], pfy()[i], pfz()[i]}; }

    void setPos(size_t i, const glm::vec3& v) { setVec3(Column::X, Column::Y, Column::Z, i, v); }
    void setVel(size_t i, const glm::vec3& v) { setVec3(Column::Vx, Column::Vy, Column::Vz, i, v); }
    void setForce(size_t i, const glm::vec3& v) { setVec3(fxCol_, fyCol_, fzCol_, i, v); }
    void setPrevForce(size_t i, const glm::vec3& v) { setVec3(pfxCol_, pfyCol_, pfzCol_, i, v); }

    void setAtomId(size_t index, AtomId id) noexcept {
        at<AtomId>(Column::Id, index) = id;
        if (static_cast<size_t>(id) >= atomIdToIndex_.size()) {
            atomIdToIndex_.resize(static_cast<size_t>(id) + 1, InvalidIndex);
        }
        atomIdToIndex_[id] = index;
    }

    [[nodiscard]] AtomId atomId(size_t index) const noexcept { return index < size() ? at<AtomId>(Column::Id, index) : InvalidAtomId; }
    [[nodiscard]] size_t indexOf(AtomId id) const noexcept { return id < atomIdToIndex_.size() ? atomIdToIndex_[id] : InvalidIndex; }
    [[nodiscard]] bool containsAtomId(AtomId id) const noexcept { return indexOf(id) != InvalidIndex; }

    void setFixed(size_t i, bool fixed) {
        if (fixed) {
            if (i >= mobileCount_) {
                return;
            }
            --mobileCount_;
            swapAtoms(i, mobileCount_);
        } else {
            if (i < mobileCount_) {
                return;
            }
            swapAtoms(i, mobileCount_);
            ++mobileCount_;
        }
    }

    void sort(SpatialGrid& grid) { sort_.mortonOrder(*this, grid); }
    [[nodiscard]] const std::vector<uint32_t>& lastSortOldToNew() const noexcept { return sort_.oldToNew(); }
};
