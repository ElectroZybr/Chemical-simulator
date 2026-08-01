#include "Lattice/Engine/physics/IForceField.h"
#include "Lattice/Engine/physics/IIntegrator.h"
#include "Lattice/Engine/physics/IThermostat.h"

ModuleRegistry<IForceField>& ForceField::registry() {
    static ModuleRegistry<IForceField> registry;
    return registry;
}

ModuleRegistry<IIntegrator>& Integrator::registry() {
    static ModuleRegistry<IIntegrator> registry;
    return registry;
}

ModuleRegistry<IThermostat>& Thermostat::registry() {
    static ModuleRegistry<IThermostat> registry;
    return registry;
}
