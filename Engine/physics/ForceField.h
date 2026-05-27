#pragma once

#include "Engine/World.h"
#include "Engine/physics/ForceFields/BondForceField.h"
#include "Engine/physics/ForceFields/CoulombForceField.h"
#include "Engine/physics/ForceFields/LJForceField.h"
#include "Engine/physics/ForceFields/WallForceField.h"

class NeighborList;

class ForceField {
public:
    ForceField();

    // Исправление бага: simStep позволяет force-entry NeighborList rebuilds
    // сохранять accurate rebuild stats после движения атомов в этом step.
    void compute(World& world, bool allowBondFormation, float dt, int simStep = 0) const;
    // Исправление бага: direct pair-interaction calls тоже обновляют NeighborList,
    // чтобы external position mutations не читали stale pair rows.
    void computePairInteractions(World& world) const;

private:
    WallForceField wallForceField_;
    LJForceField ljForceField_;
    BondForceField bondForceField_;
    CoulombForceField coulombForceField_;
};
