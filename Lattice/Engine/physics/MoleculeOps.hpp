#pragma once

#include "Lattice/Engine/physics/Atom/AtomData.h"
#include "Lattice/Engine/io/MoleculeTemplate.h"
#include "Lattice/Engine/ChemistryData/ChemistryData.hpp"

namespace MoleculeOps {
    bool calcMoleculeHybridization(MoleculeTemplate& molecule, const ChemistryData& chemistryData);
}