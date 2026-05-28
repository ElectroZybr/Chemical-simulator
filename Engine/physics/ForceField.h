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

    void compute(World& world, bool allowBondFormation, float dt) const;
    void computePairInteractions(World& world) const;

    // Доступ к LJ-таблице для GPU-режима (заливка pair-параметров в VRAM).
    [[nodiscard]] const LJForceField& ljForceField() const { return ljForceField_; }

private:
    WallForceField wallForceField_;
    LJForceField ljForceField_;
    BondForceField bondForceField_;
    CoulombForceField coulombForceField_;
};
