#pragma once
#include "Lattice/Engine/physics/Atom/AtomData.h"
#include "Lattice/Engine/ChemistryData/AngleTable.hpp"
#include "Lattice/Engine/ChemistryData/BondTable.h"

class ChemistryData {
public:
    AtomData atomData;
    BondTable bondTable;
    AngleTable angleTable;

    void init() {
        bondTable.init();
        angleTable.init();
    }
};