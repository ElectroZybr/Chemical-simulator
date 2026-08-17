// #pragma once

// #include <algorithm>
// #include <cstddef>
// #include <cstdint>
// #include <limits>
// #include <numeric>
// #include <span>
// #include <type_traits>
// #include <vector>

// #include "Plugins/ParticleDynamics/src/DynamicSoALib.hpp"
// #include "Lattice/Engine/physics/Atom/AtomData.h"

// namespace ClassicMD {
// class AtomStorage {
// public:
//     using AtomId = uint32_t;

//     AtomStorage() {
//         buffer_.add<PosX>();
//         buffer_.add<PosY>();
//         buffer_.add<PosZ>();

//         buffer_.add<VelX>();
//         buffer_.add<VelY>();
//         buffer_.add<VelZ>();

//         buffer_.add<ForceX>();
//         buffer_.add<ForceY>();
//         buffer_.add<ForceZ>();

//         buffer_.add<Energy>();
//         buffer_.add<InvMass>();
//         buffer_.add<Charge>();

//         buffer_.add<Type>();
//         buffer_.add<Valence>();
//         buffer_.add<Hybridization>();
//         buffer_.add<Id>();
//     }

// private:
//     DynamicSoA buffer_;
//     size_t mobileCount_ = 0;
//     std::vector<size_t> atomIdToIndex_;
//     AtomId nextAtomId_ = 0;
// };

// } // namespace ClassicMD