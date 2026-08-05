#include "Lattice/Engine/physics/MoleculeOps.hpp"

namespace MoleculeOps {
bool calcMoleculeHybridization(MoleculeTemplate& molecule, const ChemistryData& chemistryData) {
    for (size_t i = 0; i < molecule.atoms.size(); i++) {
        MoleculeAtom& atom = molecule.atoms[i];
        uint8_t sigma = 0;
        for (MoleculeBond& bond : molecule.bonds) {
            if (bond.atomA == i || bond.atomB == i) {
                sigma++;
            }
        }
        atom.hybridization = AtomData::Hybridization(sigma + AtomData::atomState(atom.type).lonePairs);
    }
    return true;
}
}