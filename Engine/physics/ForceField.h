#pragma once

#include "Engine/World.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/ForceFields/BondForceField.h"
#include "Engine/physics/ForceFields/CoulombForceField.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/ForceFields/WallForceField.h"

class NeighborList;

class ForceField {
public:
    ForceField();

    void compute(AtomStorage& atoms, Bond::List& bonds, World& world, NeighborList& neighborList, bool allowBondFormation, float dt) const;
    void syncWalls(const World& world);

private:
    void computePairInteractions(World& world, AtomStorage& atoms, NeighborList& neighborList) const;

    WallForceField wallForceField_;
    LJForceField ljForceField_;
    BondForceField bondForceField_;
    CoulombForceField coulombForceField_;
};