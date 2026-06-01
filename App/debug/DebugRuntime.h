#pragma once

#include <string_view>

#include "App/debug/CreateDebugPanels.h"
#include "App/debug/UpdateDebugData.h"
#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/physics/Integrator.h"

inline std::string_view integratorSchemeName(Integrator::Scheme scheme) {
    switch (scheme) {
    case Integrator::Scheme::Verlet:
        return "Velocity Verlet";
    case Integrator::Scheme::KDK:
        return "KDK (Kick-Drift-Kick)";
    case Integrator::Scheme::RK4:
        return "Runge-Kutta 4";
    case Integrator::Scheme::Langevin:
        return "Langevin";
    case Integrator::Scheme::Andersen:
        return "Andersen";
    }
    return "Unknown";
}

inline void refreshAtomDebugViews(const DebugViews& debugViews, const Lattice::Simulation& simulation) {
    updateAtomSelectionDebug(debugViews, simulation);
}

inline void refreshSimulationDebugViews(const DebugViews& debugViews, const Lattice::Simulation& simulation) {
    updateSimulationDebug(debugViews, simulation, integratorSchemeName(simulation.getIntegrator()));
}
