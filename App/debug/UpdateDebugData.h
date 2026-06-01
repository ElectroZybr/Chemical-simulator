#pragma once

#include <string_view>

namespace Lattice {
    class Simulation;
}
struct DebugViews;

void updateAtomSelectionDebug(const DebugViews& debugViews, const Lattice::Simulation& simulation);
void updateSimulationDebug(const DebugViews& debugViews, const Lattice::Simulation& simulation, std::string_view integratorName);
