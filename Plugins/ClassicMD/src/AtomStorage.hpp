#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>

#include "Plugins/ParticleDynamics/src/DynamicSoALib.hpp"
#include "Lattice/Engine/physics/Atom/AtomData.h"

namespace ClassicMD {

struct PosX {using type = float;};
struct PosY {using type = float;};
struct PosZ {using type = float;};

struct VelX {using type = float;};
struct VelY {using type = float;};
struct VelZ {using type = float;};

struct ForceX {using type = float;};
struct ForceY {using type = float;};
struct ForceZ {using type = float;};

struct Energy {using type = float;};
struct InvMass {using type = float;};
struct Charge {using type = float;};

struct Type {using type = AtomData::Type;};
struct Valence {using type = uint8_t;};
struct Hybridization {using type = AtomData::Hybridization;};
struct Id {using type = uint32_t;};

class AtomStorage {
public:
    using AtomId = uint32_t;

    AtomStorage() {
        buffer_.add<PosX>();
        buffer_.add<PosY>();
        buffer_.add<PosZ>();

        buffer_.add<VelX>();
        buffer_.add<VelY>();
        buffer_.add<VelZ>();

        buffer_.add<ForceX>();
        buffer_.add<ForceY>();
        buffer_.add<ForceZ>();

        buffer_.add<Energy>();
        buffer_.add<InvMass>();
        buffer_.add<Charge>();

        buffer_.add<Type>();
        buffer_.add<Valence>();
        buffer_.add<Hybridization>();
        buffer_.add<Id>();
    }

    template<class Tag>
    typename Tag::type* add() noexcept {
        return buffer_.add<Tag>();
    }

    template<class Tag>
    void remove() {
        buffer_.remove<Tag>();
    }

    template<class Tag>
    [[nodiscard]] typename Tag::type* get() noexcept {
        return buffer_.get<Tag>();
    }

    template<class Tag>
    [[nodiscard]] const typename Tag::type* get() const noexcept {
        return buffer_.get<Tag>();
    }

    template<class Tag>
    [[nodiscard]] typename Tag::type* require() {
        return buffer_.require<Tag>();
    }

    template<class Tag>
    [[nodiscard]] const typename Tag::type* require() const {
        return buffer_.require<Tag>();
    }

    // span
    template<class Tag>
    [[nodiscard]] std::span<typename Tag::type> span() noexcept {
        return buffer_.span<Tag>();
    }

    template<class Tag>
    [[nodiscard]] std::span<const typename Tag::type> span() const noexcept {
        return buffer_.span<Tag>();
    }

    // доступ по индексу
    template<class Tag>
    [[nodiscard]] typename Tag::type& at(size_t index) noexcept {
        return buffer_.at<Tag>(index);
    }

    template<class Tag>
    [[nodiscard]] const typename Tag::type& at(size_t index) const noexcept {
        return buffer_.at<Tag>(index);
    }

private:
    DynamicSoA buffer_;
    size_t mobileCount_ = 0;
    std::vector<size_t> atomIdToIndex_;
    AtomId nextAtomId_ = 0;
};

} // namespace ClassicMD