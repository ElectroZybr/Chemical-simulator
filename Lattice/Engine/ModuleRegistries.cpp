#include "Lattice/Engine/physics/IForceField.h"
#include "Lattice/Engine/physics/IIntegrator.h"
#include "Lattice/Engine/physics/IThermostat.h"

ModuleRegistry2<IForceField>& ForceField::registry() {
    static ModuleRegistry2<IForceField> registry;
    return registry;
}

ModuleRegistry2<IIntegrator>& Integrator::registry() {
    static ModuleRegistry2<IIntegrator> registry;
    return registry;
}

ModuleRegistry2<IThermostat>& Thermostat::registry() {
    static ModuleRegistry2<IThermostat> registry;
    return registry;
}
