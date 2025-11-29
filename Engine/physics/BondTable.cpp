#include "BondTable.h"

void BondTable::init() {
    // H–H (ковалентный)
    set(AtomType::H, AtomType::H, BondParams{0.6, 0.5, 1.0, 2.5});

    // O–H (водородная связь)
    set(AtomType::O, AtomType::H, BondParams{1.0, 0.5, 1.0, 2.5});

    // O–O (если понадобится)
    set(AtomType::O, AtomType::O, BondParams{0.5, 1.5, 2.8, 2.5});
}