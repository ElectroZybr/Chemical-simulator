#pragma once

class World;
class ChemistryData;

class IForceField {
public:
    virtual ~IForceField() = default;
    virtual bool compute(World& world, const ChemistryData& chemistryData, bool allowBondFormation, float dt) const = 0;
};