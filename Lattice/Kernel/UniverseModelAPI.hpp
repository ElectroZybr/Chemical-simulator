#pragma once

#include <string>

namespace Lattice {
class Universe;
}

struct UniverseModelAPI {
    static constexpr std::string_view apiName = "UniverseModelAPI";
    virtual void configure(Lattice::Universe& universe) = 0;
    virtual void update() = 0;
};