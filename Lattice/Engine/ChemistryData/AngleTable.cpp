#include "Lattice/Engine/ChemistryData/AngleTable.hpp"
#include "Lattice/Log.hpp"

void AngleTable::init() {
    using T = AtomData::Type;
    using H = AtomData::Hybridization;
    //    a     b     c   hy-ion  angle  stiffness
    set(T::H, T::O, T::H, H::SP3, 104.5f, 500.0f); // H-O-H water
    set(T::H, T::N, T::H, H::SP3, 107.0f, 400.0f); // H-N-H ammonia
    set(T::H, T::C, T::H, H::SP3, 109.5f, 300.0f); // C-H-C methane
    set(T::H, T::C, T::C, H::SP3, 109.5f, 300.0f); // C-C-H alkane
    set(T::C, T::C, T::C, H::SP3, 112.0f, 300.0f); // C-C-C alkane

    set(T::H, T::C, T::H, H::SP2, 120.0f, 400.0f); // C=C-H
    set(T::H, T::C, T::C, H::SP2, 120.0f, 400.0f); // C=C-C
    set(T::C, T::C, T::C, H::SP2, 120.0f, 400.0f); // C=C=C

    set(T::H, T::C, T::H, H::SP, 180.0f, 500.0f); // H-C-H linear
    set(T::H, T::C, T::C, H::SP, 180.0f, 500.0f); // C#C-H
    set(T::C, T::C, T::C, H::SP, 180.0f, 500.0f); // C#C-C
    set(T::O, T::C, T::O, H::SP, 180.0f, 600.0f); // CO2

    set(T::C, T::O, T::C, H::SP3, 111.5f, 400.0f); // ether C-O-C
    set(T::H, T::O, T::C, H::SP3, 104.5f, 400.0f); // alcohol H-O-C
    
    set(T::O, T::C, T::C, H::SP2, 120.0f, 500.0f); // carbonyl
    set(T::O, T::C, T::H, H::SP2, 120.0f, 500.0f); // aldehyde

    set(T::C, T::N, T::H, H::SP3, 107.0f, 400.0f); // amine
    set(T::C, T::N, T::C, H::SP3, 109.5f, 400.0f); // amine C-N-C
    set(T::C, T::N, T::C, H::SP2, 120.0f, 500.0f); // imine

    // карбоксильный углерод C(=O)-OH
    set(T::C, T::C, T::O, H::SP2, 120.0f, 600.0f); // Cα-C-O
    set(T::O, T::C, T::O, H::SP2, 120.0f, 600.0f); // O=C-O

    // Cα углерод CH2
    set(T::N, T::C, T::C, H::SP3, 109.5f, 500.0f); // N-Cα-C
    set(T::N, T::C, T::H, H::SP3, 109.5f, 500.0f); // N-Cα-H
    set(T::C, T::C, T::H, H::SP3, 109.5f, 500.0f); // C-Cα-H
    set(T::H, T::C, T::H, H::SP3, 109.5f, 500.0f); // H-Cα-H

    set(T::C, T::C, T::C, H::SP2, 109.5f, 500.0f); // C-C-C
    set(T::C, T::C, T::C, H::SP3, 109.5f, 500.0f); // C-C-C

    // аминогруппа NH2
    set(T::C, T::N, T::H, H::SP3, 107.0f, 450.0f); // C-N-H
    set(T::H, T::N, T::H, H::SP3, 107.0f, 450.0f); // H-N-H

    // гидроксильная группа
    set(T::C, T::O, T::H, H::SP3, 104.5f, 450.0f); // C-O-H

    Log::info("AngleTable", "angle table has been initialized");
}