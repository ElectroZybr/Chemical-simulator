#include "BondTable.h"

void BondTable::init() {
    using T = AtomData::Type;
    // H            order             r0     De    alpha
    set(T::H,  T::H,  0, BondParams{0.741f, 4.52f, 1.94f}); // H-H
    set(T::H,  T::F,  0, BondParams{0.917f, 5.87f, 2.15f}); // H-F
    set(T::H,  T::Cl, 0, BondParams{1.275f, 4.43f, 1.75f}); // H-Cl
    set(T::H,  T::O,  0, BondParams{0.957f, 4.76f, 2.20f}); // H-O
    set(T::H,  T::N,  0, BondParams{1.014f, 4.00f, 1.90f}); // H-N

    // C
    set(T::C,  T::H,  0, BondParams{1.090f, 4.28f, 2.00f}); // C-H
    set(T::C,  T::C,  0, BondParams{1.540f, 3.60f, 1.80f}); // C-C
    set(T::C,  T::C,  1, BondParams{1.340f, 6.35f, 2.10f}); // C=C
    set(T::C,  T::C,  2, BondParams{1.200f, 8.70f, 2.30f}); // C≡C
    set(T::C,  T::O,  0, BondParams{1.430f, 3.70f, 1.80f}); // C-O
    set(T::C,  T::O,  1, BondParams{1.230f, 7.70f, 2.20f}); // C=O
    set(T::C,  T::N,  0, BondParams{1.470f, 3.20f, 1.70f}); // C-N
    set(T::C,  T::N,  1, BondParams{1.280f, 6.10f, 2.00f}); // C=N
    set(T::C,  T::N,  2, BondParams{1.160f, 8.90f, 2.40f}); // C≡N
    set(T::C,  T::F,  0, BondParams{1.350f, 5.00f, 2.00f}); // C-F
    set(T::C,  T::Cl, 0, BondParams{1.770f, 3.40f, 1.70f}); // C-Cl

    // N
    set(T::N,  T::N,  0, BondParams{1.450f, 1.60f, 1.40f}); // N-N
    set(T::N,  T::N,  1, BondParams{1.250f, 4.20f, 1.80f}); // N=N
    set(T::N,  T::N,  2, BondParams{1.100f, 9.80f, 2.70f}); // N≡N
    set(T::N,  T::O,  0, BondParams{1.400f, 2.00f, 1.50f}); // N-O
    set(T::N,  T::O,  1, BondParams{1.150f, 5.00f, 2.00f}); // N=O

    // O
    set(T::O,  T::O,  0, BondParams{1.480f, 1.78f, 1.50f}); // O-O
    set(T::O,  T::O,  1, BondParams{1.210f, 5.12f, 2.00f}); // O=O
    set(T::O,  T::F,  0, BondParams{1.420f, 2.00f, 1.50f}); // O-F

    // Halogens
    set(T::F,  T::F,  0, BondParams{1.420f, 1.60f, 1.50f}); // F-F
    set(T::Cl, T::Cl, 0, BondParams{1.990f, 2.50f, 1.40f}); // Cl-Cl
    set(T::Br, T::Br, 0, BondParams{2.280f, 1.90f, 1.30f}); // Br-Br
}
