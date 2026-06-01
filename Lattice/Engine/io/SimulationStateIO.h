#pragma once

#include <string_view>

namespace Lattice {
    class Simulation;
}

class SimulationStateIO {
public:
    static void save(const Lattice::Simulation& simulation, std::string_view path);
    static void load(Lattice::Simulation& simulation, std::string_view path);
};
