#pragma once

#include <cstdint>

#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "Lattice/Engine/physics/IForceField.h"

class NeighborList;
class ChemistryData;

class BondForceField {
public:
    static void breakBond(Bond::List& bonds, Bond* bond, AtomStorage& atomStorage);
    bool compute(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList, const ChemistryData& chemistryData, bool allowBondFormation, float dt) const;

private:
    bool formBonds(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList, const ChemistryData& chemistryData) const;
    bool tryCreateBond(AtomStorage& atoms, Bond::List& bonds, uint32_t aIndex, uint32_t bIndex, const ChemistryData& chemistryData) const;
    static bool shouldBreak(const Bond& bond, const AtomStorage& atoms);
    static void detachBond(const Bond& bond, AtomStorage& atomStorage);
    static float morseForce(const Bond& bond, float distance);
    static void applyBondForce(const Bond& bond, AtomStorage& atomStorage, float dt);
    static void applyAngleForce(AtomStorage& atomStorage, size_t aIndex, size_t bIndex, size_t cIndex, const ChemistryData& chemistryData);
    static void applyAngleForces(AtomStorage& atoms, const Bond::List& bonds, const ChemistryData& chemistryData);
};
