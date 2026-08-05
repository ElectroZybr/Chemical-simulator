#pragma once

class ForceField;
class IThermostat;
class NeighborList;
class ChemistryData;
class World;

struct StepContext {
    World& world;
    ForceField& forceField;
    NeighborList& neighborList;
    IThermostat* thermostat = nullptr;
    ChemistryData& chemistryData;
    bool allowBondFormation;
    bool bondsChanged = false;
    float dt;
};
