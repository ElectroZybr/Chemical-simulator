#include "AtomData.h"

namespace {
    constexpr Color atomColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return Color{static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8u) | (static_cast<uint32_t>(b) << 16u) |
                     (static_cast<uint32_t>(a) << 24u)};
    }
}

constexpr std::array<std::string_view, static_cast<size_t>(AtomData::Type::COUNT)> kAtomSymbols = {{
    "Z",
    "H", "He",
    "Li", "Be", "B", "C", "N", "O", "F", "Ne",
    "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar",
    "K", "Ca", "Sc", "Ti", "V", "Cr", "Mn", "Fe",
    "Co", "Ni", "Cu", "Zn", "Ga", "Ge", "As", "Se", "Br", "Kr",
    "Rb", "Sr", "Y", "Zr", "Nb", "Mo", "Tc", "Ru",
    "Rh", "Pd", "Ag", "Cd", "In", "Sn", "Sb", "Te", "I", "Xe",
    "Cs", "Ba", "La", "Ce", "Pr", "Nd", "Pm", "Sm",
    "Eu", "Gd", "Tb", "Dy", "Ho", "Er", "Tm", "Yb",
    "Lu", "Hf", "Ta", "W", "Re", "Os", "Ir", "Pt",
    "Au", "Hg", "Tl", "Pb", "Bi", "Po", "At", "Rn",
    "Fr", "Ra", "Ac", "Th", "Pa", "U", "Np", "Pu",
    "Am", "Cm", "Bk", "Cf", "Es", "Fm", "Md", "No",
    "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds",
    "Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og",
}};

const std::array<StaticAtomicData, static_cast<size_t>(AtomData::Type::COUNT)> AtomData::properties = {{
    {1.0000f, 0.5f, 1, 0.0f, atomColor(0, 230, 170, 255), 2.00f, 15.0f}, // Ze - Zerium

    {1.0080f, 0.5f, 1, 0.0f, atomColor(255, 255, 255, 255), 2.40f, 0.03f}, // H  - Hydrogen
    {4.0026f, 0.5f, 0, 0.0f, atomColor(217, 255, 255, 255), 2.60f, 0.02f}, // He - Helium

    {6.9400f, 0.5f, 1, 0.0f, atomColor(204, 128, 255, 255), 3.60f, 0.09f}, // Li - Lithium
    {9.0122f, 0.5f, 2, 0.0f, atomColor(194, 255, 0, 255), 3.20f, 0.10f},   // Be - Beryllium
    {10.810f, 0.5f, 3, 0.0f, atomColor(255, 181, 181, 255), 3.00f, 0.10f}, // B  - Boron
    {12.011f, 0.5f, 4, 0.0f, atomColor(144, 144, 144, 255), 3.40f, 0.12f}, // C  - Carbon
    {14.007f, 0.5f, 5, 0.0f, atomColor(48, 80, 248, 255), 3.20f, 0.11f},   // N  - Nitrogen
    {15.999f, 0.5f, 2, 0.0f, atomColor(255, 13, 13, 255), 3.00f, 0.10f},   // O  - Oxygen
    {18.998f, 0.5f, 1, 0.0f, atomColor(144, 224, 80, 255), 3.00f, 0.08f},  // F  - Fluorine
    {20.180f, 0.5f, 0, 0.0f, atomColor(179, 227, 245, 255), 2.80f, 0.03f}, // Ne - Neon

    {22.990f, 0.5f, 1, 0.0f, atomColor(171, 92, 242, 255), 4.00f, 0.12f},  // Na - Sodium
    {24.305f, 0.5f, 2, 0.0f, atomColor(138, 255, 0, 255), 3.60f, 0.13f},   // Mg - Magnesium
    {26.982f, 0.5f, 3, 0.0f, atomColor(191, 166, 166, 255), 3.40f, 0.14f}, // Al - Aluminum
    {28.085f, 0.5f, 4, 0.0f, atomColor(240, 200, 160, 255), 3.30f, 0.15f}, // Si - Silicon
    {30.974f, 0.5f, 5, 0.0f, atomColor(255, 128, 0, 255), 3.20f, 0.16f},   // P  - Phosphorus
    {32.060f, 0.5f, 6, 0.0f, atomColor(255, 255, 48, 255), 3.20f, 0.18f},  // S  - Sulfur
    {35.450f, 0.5f, 7, 0.0f, atomColor(31, 240, 31, 255), 3.10f, 0.15f},   // Cl - Chlorine
    {39.948f, 0.5f, 0, 0.0f, atomColor(128, 209, 227, 255), 3.00f, 0.07f}, // Ar - Argon

    {39.098f, 0.5f, 1, 0.0f, atomColor(143, 64, 212, 255), 4.80f, 0.18f},  // K  - Potassium
    {40.078f, 0.5f, 2, 0.0f, atomColor(61, 255, 0, 255), 4.40f, 0.17f},    // Ca - Calcium
    {44.956f, 0.5f, 3, 0.0f, atomColor(230, 230, 230, 255), 4.10f, 0.17f}, // Sc - Scandium
    {47.867f, 0.5f, 4, 0.0f, atomColor(191, 194, 199, 255), 4.00f, 0.18f}, // Ti - Titanium
    {50.942f, 0.5f, 5, 0.0f, atomColor(166, 166, 171, 255), 3.90f, 0.18f}, // V  - Vanadium
    {51.996f, 0.5f, 6, 0.0f, atomColor(138, 153, 199, 255), 3.80f, 0.18f}, // Cr - Chromium
    {54.938f, 0.5f, 7, 0.0f, atomColor(156, 122, 199, 255), 3.70f, 0.18f}, // Mn - Manganese
    {55.845f, 0.5f, 3, 0.0f, atomColor(224, 102, 51, 255), 3.60f, 0.19f},  // Fe - Iron
    {58.933f, 0.5f, 3, 0.0f, atomColor(240, 144, 160, 255), 3.60f, 0.20f}, // Co - Cobalt
    {58.693f, 0.5f, 2, 0.0f, atomColor(80, 208, 80, 255), 3.50f, 0.21f},   // Ni - Nickel
    {63.546f, 0.5f, 2, 0.0f, atomColor(200, 128, 51, 255), 3.40f, 0.20f},  // Cu - Copper
    {65.380f, 0.5f, 2, 0.0f, atomColor(125, 128, 176, 255), 3.30f, 0.18f}, // Zn - Zinc
    {69.723f, 0.5f, 3, 0.0f, atomColor(194, 143, 143, 255), 3.20f, 0.18f}, // Ga - Gallium
    {72.630f, 0.5f, 4, 0.0f, atomColor(102, 143, 143, 255), 3.10f, 0.17f}, // Ge - Germanium
    {74.922f, 0.5f, 5, 0.0f, atomColor(189, 128, 227, 255), 3.00f, 0.16f}, // As - Arsenic
    {78.971f, 0.5f, 6, 0.0f, atomColor(255, 161, 0, 255), 3.00f, 0.16f},   // Se - Selenium
    {79.904f, 0.5f, 7, 0.0f, atomColor(166, 41, 41, 255), 3.00f, 0.15f},   // Br - Bromine
    {83.798f, 0.5f, 0, 0.0f, atomColor(92, 184, 209, 255), 2.90f, 0.08f},  // Kr - Krypton

    {85.468f, 0.5f, 1, 0.0f, atomColor(143, 64, 212, 255), 4.90f, 0.20f},  // Rb - Rubidium
    {87.620f, 0.5f, 2, 0.0f, atomColor(61, 255, 0, 255), 4.60f, 0.19f},    // Sr - Strontium
    {88.906f, 0.5f, 3, 0.0f, atomColor(230, 230, 230, 255), 4.30f, 0.19f}, // Y  - Yttrium
    {91.224f, 0.5f, 4, 0.0f, atomColor(191, 194, 199, 255), 4.20f, 0.19f}, // Zr - Zirconium
    {92.906f, 0.5f, 5, 0.0f, atomColor(166, 166, 171, 255), 4.10f, 0.19f}, // Nb - Niobium
    {95.950f, 0.5f, 6, 0.0f, atomColor(138, 153, 199, 255), 4.00f, 0.19f}, // Mo - Molybdenum
    {98.000f, 0.5f, 7, 0.0f, atomColor(156, 122, 199, 255), 3.90f, 0.19f}, // Tc - Technetium
    {101.07f, 0.5f, 3, 0.0f, atomColor(224, 102, 51, 255), 3.80f, 0.20f},  // Ru - Ruthenium
    {102.91f, 0.5f, 3, 0.0f, atomColor(240, 144, 160, 255), 3.70f, 0.20f}, // Rh - Rhodium
    {106.42f, 0.5f, 2, 0.0f, atomColor(80, 208, 80, 255), 3.60f, 0.20f},   // Pd - Palladium
    {107.87f, 0.5f, 1, 0.0f, atomColor(200, 128, 51, 255), 3.50f, 0.20f},  // Ag - Silver
    {112.41f, 0.5f, 2, 0.0f, atomColor(125, 128, 176, 255), 3.40f, 0.19f}, // Cd - Cadmium
    {114.82f, 0.5f, 3, 0.0f, atomColor(194, 143, 143, 255), 3.30f, 0.18f}, // In - Indium
    {118.71f, 0.5f, 4, 0.0f, atomColor(194, 143, 143, 255), 3.20f, 0.18f}, // Sn - Tin
    {121.76f, 0.5f, 5, 0.0f, atomColor(189, 128, 227, 255), 3.10f, 0.17f}, // Sb - Antimony
    {127.60f, 0.5f, 6, 0.0f, atomColor(255, 161, 0, 255), 3.10f, 0.17f},   // Te - Tellurium
    {126.90f, 0.5f, 7, 0.0f, atomColor(166, 41, 41, 255), 3.00f, 0.16f},   // I  - Iodine
    {131.29f, 0.5f, 0, 0.0f, atomColor(92, 184, 209, 255), 3.00f, 0.09f},  // Xe - Xenon

    {132.91f, 0.5f, 1, 0.0f, atomColor(143, 64, 212, 255), 5.20f, 0.22f},  // Cs - Cesium
    {137.33f, 0.5f, 2, 0.0f, atomColor(61, 255, 0, 255), 4.90f, 0.21f},    // Ba - Barium
    {138.91f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.60f, 0.20f}, // La - Lanthanum
    {140.12f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.50f, 0.20f}, // Ce - Cerium
    {140.91f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.40f, 0.20f}, // Pr - Praseodymium
    {144.24f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.30f, 0.20f}, // Nd - Neodymium
    {145.00f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.20f, 0.20f}, // Pm - Promethium
    {150.36f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.10f, 0.20f}, // Sm - Samarium
    {151.96f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.00f, 0.20f}, // Eu - Europium
    {157.25f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.90f, 0.20f}, // Gd - Gadolinium
    {158.93f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.80f, 0.20f}, // Tb - Terbium
    {162.50f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.70f, 0.20f}, // Dy - Dysprosium
    {164.93f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.70f, 0.20f}, // Ho - Holmium
    {167.26f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.60f, 0.20f}, // Er - Erbium
    {168.93f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.60f, 0.20f}, // Tm - Thulium
    {173.05f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.50f, 0.20f}, // Yb - Ytterbium
    {174.97f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.50f, 0.20f}, // Lu - Lutetium
    {178.49f, 0.5f, 4, 0.0f, atomColor(191, 194, 199, 255), 3.40f, 0.20f}, // Hf - Hafnium
    {180.95f, 0.5f, 5, 0.0f, atomColor(166, 166, 171, 255), 3.40f, 0.20f}, // Ta - Tantalum
    {183.84f, 0.5f, 6, 0.0f, atomColor(138, 153, 199, 255), 3.30f, 0.20f}, // W  - Tungsten
    {186.21f, 0.5f, 7, 0.0f, atomColor(156, 122, 199, 255), 3.30f, 0.20f}, // Re - Rhenium
    {190.23f, 0.5f, 4, 0.0f, atomColor(224, 102, 51, 255), 3.20f, 0.20f},  // Os - Osmium
    {192.22f, 0.5f, 4, 0.0f, atomColor(240, 144, 160, 255), 3.20f, 0.20f}, // Ir - Iridium
    {195.08f, 0.5f, 4, 0.0f, atomColor(80, 208, 80, 255), 3.20f, 0.20f},   // Pt - Platinum
    {196.97f, 0.5f, 3, 0.0f, atomColor(200, 128, 51, 255), 3.10f, 0.20f},  // Au - Gold
    {200.59f, 0.5f, 2, 0.0f, atomColor(125, 128, 176, 255), 3.10f, 0.19f}, // Hg - Mercury
    {204.38f, 0.5f, 3, 0.0f, atomColor(194, 143, 143, 255), 3.10f, 0.18f}, // Tl - Thallium
    {207.20f, 0.5f, 4, 0.0f, atomColor(194, 143, 143, 255), 3.10f, 0.18f}, // Pb - Lead
    {208.98f, 0.5f, 5, 0.0f, atomColor(194, 143, 143, 255), 3.10f, 0.18f}, // Bi - Bismuth
    {209.00f, 0.5f, 6, 0.0f, atomColor(255, 161, 0, 255), 3.00f, 0.17f},   // Po - Polonium
    {210.00f, 0.5f, 7, 0.0f, atomColor(166, 41, 41, 255), 3.00f, 0.16f},   // At - Astatine
    {222.00f, 0.5f, 0, 0.0f, atomColor(92, 184, 209, 255), 3.00f, 0.10f},  // Rn - Radon

    {223.00f, 0.5f, 1, 0.0f, atomColor(143, 64, 212, 255), 5.30f, 0.22f},  // Fr - Francium
    {226.00f, 0.5f, 2, 0.0f, atomColor(61, 255, 0, 255), 5.00f, 0.21f},    // Ra - Radium
    {227.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 4.80f, 0.21f}, // Ac - Actinium
    {232.04f, 0.5f, 4, 0.0f, atomColor(112, 170, 250, 255), 4.70f, 0.21f}, // Th - Thorium
    {231.04f, 0.5f, 5, 0.0f, atomColor(112, 170, 250, 255), 4.60f, 0.21f}, // Pa - Protactinium
    {238.03f, 0.5f, 6, 0.0f, atomColor(112, 170, 250, 255), 4.50f, 0.21f}, // U  - Uranium
    {237.00f, 0.5f, 5, 0.0f, atomColor(112, 170, 250, 255), 4.40f, 0.21f}, // Np - Neptunium
    {244.00f, 0.5f, 4, 0.0f, atomColor(112, 170, 250, 255), 4.30f, 0.21f}, // Pu - Plutonium
    {243.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 4.20f, 0.21f}, // Am - Americium
    {247.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 4.10f, 0.21f}, // Cm - Curium
    {247.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 4.00f, 0.21f}, // Bk - Berkelium
    {251.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.90f, 0.21f}, // Cf - Californium
    {252.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.90f, 0.21f}, // Es - Einsteinium
    {257.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.80f, 0.21f}, // Fm - Fermium
    {258.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.80f, 0.21f}, // Md - Mendelevium
    {259.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.70f, 0.21f}, // No - Nobelium
    {266.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.70f, 0.21f}, // Lr - Lawrencium
    {267.00f, 0.5f, 4, 0.0f, atomColor(170, 170, 170, 255), 3.60f, 0.21f}, // Rf - Rutherfordium
    {268.00f, 0.5f, 5, 0.0f, atomColor(170, 170, 170, 255), 3.60f, 0.21f}, // Db - Dubnium
    {269.00f, 0.5f, 6, 0.0f, atomColor(170, 170, 170, 255), 3.60f, 0.21f}, // Sg - Seaborgium
    {270.00f, 0.5f, 7, 0.0f, atomColor(170, 170, 170, 255), 3.50f, 0.21f}, // Bh - Bohrium
    {269.00f, 0.5f, 4, 0.0f, atomColor(170, 170, 170, 255), 3.50f, 0.21f}, // Hs - Hassium
    {278.00f, 0.5f, 3, 0.0f, atomColor(170, 170, 170, 255), 3.50f, 0.21f}, // Mt - Meitnerium
    {281.00f, 0.5f, 2, 0.0f, atomColor(170, 170, 170, 255), 3.40f, 0.21f}, // Ds - Darmstadtium
    {282.00f, 0.5f, 1, 0.0f, atomColor(170, 170, 170, 255), 3.40f, 0.21f}, // Rg - Roentgenium
    {285.00f, 0.5f, 2, 0.0f, atomColor(170, 170, 170, 255), 3.40f, 0.20f}, // Cn - Copernicium
    {286.00f, 0.5f, 3, 0.0f, atomColor(170, 170, 170, 255), 3.30f, 0.20f}, // Nh - Nihonium
    {289.00f, 0.5f, 4, 0.0f, atomColor(170, 170, 170, 255), 3.30f, 0.20f}, // Fl - Flerovium
    {290.00f, 0.5f, 5, 0.0f, atomColor(170, 170, 170, 255), 3.20f, 0.20f}, // Mc - Moscovium
    {293.00f, 0.5f, 6, 0.0f, atomColor(170, 170, 170, 255), 3.20f, 0.20f}, // Lv - Livermorium
    {294.00f, 0.5f, 7, 0.0f, atomColor(170, 170, 170, 255), 3.10f, 0.19f}, // Ts - Tennessine
    {294.00f, 0.5f, 0, 0.0f, atomColor(170, 170, 170, 255), 3.10f, 0.10f}, // Og - Oganesson
}};

std::string_view AtomData::symbol(Type type) {
    const size_t index = static_cast<size_t>(type);
    if (index >= kAtomSymbols.size()) {
        return "X";
    }
    return kAtomSymbols[index];
}
