#include "BondTable.h"
#include <Lattice/Tools/Logger.hpp>

void BondTable::init() {
    using T = AtomData::Type;
    using O = AtomData::Order;
    
    // H                order                 r0      k    
    set(T::H,  T::H,  O::Single, BondParams{0.741f, 300.0f}); // H-H
    set(T::H,  T::F,  O::Single, BondParams{0.917f, 350.0f}); // H-F
    set(T::H,  T::Cl, O::Single, BondParams{1.275f, 250.0f}); // H-Cl
    set(T::H,  T::O,  O::Single, BondParams{0.957f, 450.0f}); // H-O
    set(T::H,  T::N,  O::Single, BondParams{1.014f, 350.0f}); // H-N

    // C
    set(T::C,  T::H,  O::Single, BondParams{1.090f, 350.0f}); // C-H
    set(T::C,  T::C,  O::Single, BondParams{1.540f, 300.0f}); // C-C
    set(T::C,  T::C,  O::Double, BondParams{1.340f, 600.0f}); // C=C
    set(T::C,  T::C,  O::Triple, BondParams{1.200f, 900.0f}); // C≡C
    set(T::C,  T::O,  O::Single, BondParams{1.430f, 350.0f}); // C-O
    set(T::C,  T::O,  O::Double, BondParams{1.230f, 700.0f}); // C=O
    set(T::C,  T::N,  O::Single, BondParams{1.470f, 300.0f}); // C-N
    set(T::C,  T::N,  O::Double, BondParams{1.280f, 600.0f}); // C=N
    set(T::C,  T::N,  O::Triple, BondParams{1.160f, 900.0f}); // C≡N
    set(T::C,  T::F,  O::Single, BondParams{1.350f, 400.0f}); // C-F
    set(T::C,  T::Cl, O::Single, BondParams{1.770f, 250.0f}); // C-Cl

    // N
    set(T::N,  T::N,  O::Single, BondParams{1.450f, 200.0f}); // N-N
    set(T::N,  T::N,  O::Double, BondParams{1.250f, 500.0f}); // N=N
    set(T::N,  T::N,  O::Triple, BondParams{1.100f, 1000.f}); // N≡N
    set(T::N,  T::O,  O::Single, BondParams{1.400f, 250.0f}); // N-O
    set(T::N,  T::O,  O::Double, BondParams{1.150f, 600.0f}); // N=O

    // O
    set(T::O,  T::O,  O::Single, BondParams{1.480f, 200.0f}); // O-O
    set(T::O,  T::O,  O::Double, BondParams{1.210f, 700.0f}); // O=O
    set(T::O,  T::F,  O::Single, BondParams{1.420f, 250.0f}); // O-F

    // Halogens
    set(T::F,  T::F,  O::Single, BondParams{1.420f, 200.0f}); // F-F
    set(T::Cl, T::Cl, O::Single, BondParams{1.990f, 150.0f}); // Cl-Cl
    set(T::Br, T::Br, O::Single, BondParams{2.280f, 120.0f}); // Br-Br

    Logger::info("BondTable", "bond table has been initialized");
}
