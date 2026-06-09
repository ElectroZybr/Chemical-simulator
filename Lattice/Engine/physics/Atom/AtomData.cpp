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

constexpr std::array<std::string_view, static_cast<size_t>(AtomData::Type::COUNT)> kAtomNames = {{
    "Zerium",
    "Hydrogen", "Helium",
    "Lithium", "Beryllium", "Boron", "Carbon", "Nitrogen", "Oxygen", "Fluorine", "Neon",
    "Sodium", "Magnesium", "Aluminum", "Silicon", "Phosphorus", "Sulfur", "Chlorine", "Argon",
    "Potassium", "Calcium", "Scandium", "Titanium", "Vanadium", "Chromium", "Manganese", "Iron",
    "Cobalt", "Nickel", "Copper", "Zinc", "Gallium", "Germanium", "Arsenic", "Selenium", "Bromine", "Krypton",
    "Rubidium", "Strontium", "Yttrium", "Zirconium", "Niobium", "Molybdenum", "Technetium", "Ruthenium",
    "Rhodium", "Palladium", "Silver", "Cadmium", "Indium", "Tin", "Antimony", "Tellurium", "Iodine", "Xenon",
    "Cesium", "Barium", "Lanthanum", "Cerium", "Praseodymium", "Neodymium", "Promethium", "Samarium",
    "Europium", "Gadolinium", "Terbium", "Dysprosium", "Holmium", "Erbium", "Thulium", "Ytterbium",
    "Lutetium", "Hafnium", "Tantalum", "Tungsten", "Rhenium", "Osmium", "Iridium", "Platinum",
    "Gold", "Mercury", "Thallium", "Lead", "Bismuth", "Polonium", "Astatine", "Radon",
    "Francium", "Radium", "Actinium", "Thorium", "Protactinium", "Uranium", "Neptunium", "Plutonium",
    "Americium", "Curium", "Berkelium", "Californium", "Einsteinium", "Fermium", "Mendelevium", "Nobelium",
    "Lawrencium", "Rutherfordium", "Dubnium", "Seaborgium", "Bohrium", "Hassium", "Meitnerium", "Darmstadtium",
    "Roentgenium", "Copernicium", "Nihonium", "Flerovium", "Moscovium", "Livermorium", "Tennessine", "Oganesson",
}};

const std::array<StaticAtomicData, static_cast<size_t>(AtomData::Type::COUNT)> AtomData::properties = {{
    {1.0000f, 0.5f, 1, 0.0f, atomColor(0, 230, 170, 255), 2.00f, 15.0f, AtomData::Category::Custom}, // Ze - Zerium

    {1.0080f, 0.5f, 1, 0.42f, atomColor(255, 255, 255, 255), 2.40f, 0.03f, AtomData::Category::ReactiveNonmetal}, // H  - Hydrogen
    {4.0026f, 0.5f, 0, 0.0f, atomColor(217, 255, 255, 255), 2.60f, 0.02f, AtomData::Category::NobleGas},         // He - Helium

    {6.9400f, 0.5f, 1, 0.0f, atomColor(204, 128, 255, 255), 3.60f, 0.09f, AtomData::Category::AlkaliMetal},        // Li - Lithium
    {9.0122f, 0.5f, 2, 0.0f, atomColor(194, 255, 0, 255), 3.20f, 0.10f, AtomData::Category::AlkalineEarthMetal},   // Be - Beryllium
    {10.810f, 0.5f, 3, 0.0f, atomColor(255, 181, 181, 255), 3.00f, 0.10f, AtomData::Category::Metalloid},          // B  - Boron
    {12.011f, 0.5f, 4, 0.0f, atomColor(144, 144, 144, 255), 3.40f, 0.12f, AtomData::Category::ReactiveNonmetal},   // C  - Carbon
    {14.007f, 0.5f, 5, 0.0f, atomColor(48, 80, 248, 255), 3.20f, 0.11f, AtomData::Category::ReactiveNonmetal},     // N  - Nitrogen
    {15.999f, 0.5f, 2, -0.84f, atomColor(255, 13, 13, 255), 3.00f, 0.10f, AtomData::Category::ReactiveNonmetal},   // O  - Oxygen
    {18.998f, 0.5f, 1, 0.0f, atomColor(144, 224, 80, 255), 3.00f, 0.08f, AtomData::Category::Halogen},             // F  - Fluorine
    {20.180f, 0.5f, 0, 0.0f, atomColor(179, 227, 245, 255), 2.80f, 0.03f, AtomData::Category::NobleGas},          // Ne - Neon

    {22.990f, 0.5f, 1, 1.0f, atomColor(171, 92, 242, 255), 4.00f, 0.12f, AtomData::Category::AlkaliMetal},         // Na - Sodium
    {24.305f, 0.5f, 2, 0.0f, atomColor(138, 255, 0, 255), 3.60f, 0.13f, AtomData::Category::AlkalineEarthMetal},   // Mg - Magnesium
    {26.982f, 0.5f, 3, 0.0f, atomColor(191, 166, 166, 255), 3.40f, 0.14f, AtomData::Category::PostTransitionMetal}, // Al - Aluminum
    {28.085f, 0.5f, 4, 0.0f, atomColor(240, 200, 160, 255), 3.30f, 0.15f, AtomData::Category::Metalloid},          // Si - Silicon
    {30.974f, 0.5f, 5, 0.0f, atomColor(255, 128, 0, 255), 3.20f, 0.16f, AtomData::Category::ReactiveNonmetal},     // P  - Phosphorus
    {32.060f, 0.5f, 6, 0.0f, atomColor(255, 255, 48, 255), 3.20f, 0.18f, AtomData::Category::ReactiveNonmetal},    // S  - Sulfur
    {35.450f, 0.5f, 7, -1.0f, atomColor(31, 240, 31, 255), 3.10f, 0.15f, AtomData::Category::Halogen},             // Cl - Chlorine
    {39.948f, 0.5f, 0, 0.0f, atomColor(128, 209, 227, 255), 3.00f, 0.07f, AtomData::Category::NobleGas},          // Ar - Argon

    {39.098f, 0.5f, 1, 0.0f, atomColor(143, 64, 212, 255), 4.80f, 0.18f, AtomData::Category::AlkaliMetal},         // K  - Potassium
    {40.078f, 0.5f, 2, 0.0f, atomColor(61, 255, 0, 255), 4.40f, 0.17f, AtomData::Category::AlkalineEarthMetal},    // Ca - Calcium
    {44.956f, 0.5f, 3, 0.0f, atomColor(230, 230, 230, 255), 4.10f, 0.17f, AtomData::Category::TransitionMetal},    // Sc - Scandium
    {47.867f, 0.5f, 4, 0.0f, atomColor(191, 194, 199, 255), 4.00f, 0.18f, AtomData::Category::TransitionMetal},    // Ti - Titanium
    {50.942f, 0.5f, 5, 0.0f, atomColor(166, 166, 171, 255), 3.90f, 0.18f, AtomData::Category::TransitionMetal},    // V  - Vanadium
    {51.996f, 0.5f, 6, 0.0f, atomColor(138, 153, 199, 255), 3.80f, 0.18f, AtomData::Category::TransitionMetal},    // Cr - Chromium
    {54.938f, 0.5f, 7, 0.0f, atomColor(156, 122, 199, 255), 3.70f, 0.18f, AtomData::Category::TransitionMetal},    // Mn - Manganese
    {55.845f, 0.5f, 3, 0.0f, atomColor(224, 102, 51, 255), 3.60f, 0.19f, AtomData::Category::TransitionMetal},     // Fe - Iron
    {58.933f, 0.5f, 3, 0.0f, atomColor(240, 144, 160, 255), 3.60f, 0.20f, AtomData::Category::TransitionMetal},    // Co - Cobalt
    {58.693f, 0.5f, 2, 0.0f, atomColor(80, 208, 80, 255), 3.50f, 0.21f, AtomData::Category::TransitionMetal},      // Ni - Nickel
    {63.546f, 0.5f, 2, 0.0f, atomColor(200, 128, 51, 255), 3.40f, 0.20f, AtomData::Category::TransitionMetal},     // Cu - Copper
    {65.380f, 0.5f, 2, 0.0f, atomColor(125, 128, 176, 255), 3.30f, 0.18f, AtomData::Category::TransitionMetal},    // Zn - Zinc
    {69.723f, 0.5f, 3, 0.0f, atomColor(194, 143, 143, 255), 3.20f, 0.18f, AtomData::Category::PostTransitionMetal}, // Ga - Gallium
    {72.630f, 0.5f, 4, 0.0f, atomColor(102, 143, 143, 255), 3.10f, 0.17f, AtomData::Category::Metalloid},          // Ge - Germanium
    {74.922f, 0.5f, 5, 0.0f, atomColor(189, 128, 227, 255), 3.00f, 0.16f, AtomData::Category::Metalloid},          // As - Arsenic
    {78.971f, 0.5f, 6, 0.0f, atomColor(255, 161, 0, 255), 3.00f, 0.16f, AtomData::Category::ReactiveNonmetal},    // Se - Selenium
    {79.904f, 0.5f, 7, 0.0f, atomColor(166, 41, 41, 255), 3.00f, 0.15f, AtomData::Category::Halogen},             // Br - Bromine
    {83.798f, 0.5f, 0, 0.0f, atomColor(92, 184, 209, 255), 2.90f, 0.08f, AtomData::Category::NobleGas},           // Kr - Krypton

    {85.468f, 0.5f, 1, 0.0f, atomColor(143, 64, 212, 255), 4.90f, 0.20f, AtomData::Category::AlkaliMetal},         // Rb - Rubidium
    {87.620f, 0.5f, 2, 0.0f, atomColor(61, 255, 0, 255), 4.60f, 0.19f, AtomData::Category::AlkalineEarthMetal},    // Sr - Strontium
    {88.906f, 0.5f, 3, 0.0f, atomColor(230, 230, 230, 255), 4.30f, 0.19f, AtomData::Category::TransitionMetal},    // Y  - Yttrium
    {91.224f, 0.5f, 4, 0.0f, atomColor(191, 194, 199, 255), 4.20f, 0.19f, AtomData::Category::TransitionMetal},    // Zr - Zirconium
    {92.906f, 0.5f, 5, 0.0f, atomColor(166, 166, 171, 255), 4.10f, 0.19f, AtomData::Category::TransitionMetal},    // Nb - Niobium
    {95.950f, 0.5f, 6, 0.0f, atomColor(138, 153, 199, 255), 4.00f, 0.19f, AtomData::Category::TransitionMetal},    // Mo - Molybdenum
    {98.000f, 0.5f, 7, 0.0f, atomColor(156, 122, 199, 255), 3.90f, 0.19f, AtomData::Category::TransitionMetal},    // Tc - Technetium
    {101.07f, 0.5f, 3, 0.0f, atomColor(224, 102, 51, 255), 3.80f, 0.20f, AtomData::Category::TransitionMetal},     // Ru - Ruthenium
    {102.91f, 0.5f, 3, 0.0f, atomColor(240, 144, 160, 255), 3.70f, 0.20f, AtomData::Category::TransitionMetal},    // Rh - Rhodium
    {106.42f, 0.5f, 2, 0.0f, atomColor(80, 208, 80, 255), 3.60f, 0.20f, AtomData::Category::TransitionMetal},      // Pd - Palladium
    {107.87f, 0.5f, 1, 0.0f, atomColor(200, 128, 51, 255), 3.50f, 0.20f, AtomData::Category::TransitionMetal},     // Ag - Silver
    {112.41f, 0.5f, 2, 0.0f, atomColor(125, 128, 176, 255), 3.40f, 0.19f, AtomData::Category::TransitionMetal},    // Cd - Cadmium
    {114.82f, 0.5f, 3, 0.0f, atomColor(194, 143, 143, 255), 3.30f, 0.18f, AtomData::Category::PostTransitionMetal}, // In - Indium
    {118.71f, 0.5f, 4, 0.0f, atomColor(194, 143, 143, 255), 3.20f, 0.18f, AtomData::Category::PostTransitionMetal}, // Sn - Tin
    {121.76f, 0.5f, 5, 0.0f, atomColor(189, 128, 227, 255), 3.10f, 0.17f, AtomData::Category::Metalloid},          // Sb - Antimony
    {127.60f, 0.5f, 6, 0.0f, atomColor(255, 161, 0, 255), 3.10f, 0.17f, AtomData::Category::Metalloid},           // Te - Tellurium
    {126.90f, 0.5f, 7, 0.0f, atomColor(166, 41, 41, 255), 3.00f, 0.16f, AtomData::Category::Halogen},             // I  - Iodine
    {131.29f, 0.5f, 0, 0.0f, atomColor(92, 184, 209, 255), 3.00f, 0.09f, AtomData::Category::NobleGas},           // Xe - Xenon

    {132.91f, 0.5f, 1, 0.0f, atomColor(143, 64, 212, 255), 5.20f, 0.22f, AtomData::Category::AlkaliMetal},         // Cs - Cesium
    {137.33f, 0.5f, 2, 0.0f, atomColor(61, 255, 0, 255), 4.90f, 0.21f, AtomData::Category::AlkalineEarthMetal},    // Ba - Barium
    {138.91f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.60f, 0.20f, AtomData::Category::Lanthanide},         // La - Lanthanum
    {140.12f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.50f, 0.20f, AtomData::Category::Lanthanide},         // Ce - Cerium
    {140.91f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.40f, 0.20f, AtomData::Category::Lanthanide},         // Pr - Praseodymium
    {144.24f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.30f, 0.20f, AtomData::Category::Lanthanide},         // Nd - Neodymium
    {145.00f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.20f, 0.20f, AtomData::Category::Lanthanide},         // Pm - Promethium
    {150.36f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.10f, 0.20f, AtomData::Category::Lanthanide},         // Sm - Samarium
    {151.96f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 4.00f, 0.20f, AtomData::Category::Lanthanide},         // Eu - Europium
    {157.25f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.90f, 0.20f, AtomData::Category::Lanthanide},         // Gd - Gadolinium
    {158.93f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.80f, 0.20f, AtomData::Category::Lanthanide},         // Tb - Terbium
    {162.50f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.70f, 0.20f, AtomData::Category::Lanthanide},         // Dy - Dysprosium
    {164.93f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.70f, 0.20f, AtomData::Category::Lanthanide},         // Ho - Holmium
    {167.26f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.60f, 0.20f, AtomData::Category::Lanthanide},         // Er - Erbium
    {168.93f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.60f, 0.20f, AtomData::Category::Lanthanide},         // Tm - Thulium
    {173.05f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.50f, 0.20f, AtomData::Category::Lanthanide},         // Yb - Ytterbium
    {174.97f, 0.5f, 3, 0.0f, atomColor(112, 212, 255, 255), 3.50f, 0.20f, AtomData::Category::Lanthanide},         // Lu - Lutetium
    {178.49f, 0.5f, 4, 0.0f, atomColor(191, 194, 199, 255), 3.40f, 0.20f, AtomData::Category::TransitionMetal},    // Hf - Hafnium
    {180.95f, 0.5f, 5, 0.0f, atomColor(166, 166, 171, 255), 3.40f, 0.20f, AtomData::Category::TransitionMetal},    // Ta - Tantalum
    {183.84f, 0.5f, 6, 0.0f, atomColor(138, 153, 199, 255), 3.30f, 0.20f, AtomData::Category::TransitionMetal},    // W  - Tungsten
    {186.21f, 0.5f, 7, 0.0f, atomColor(156, 122, 199, 255), 3.30f, 0.20f, AtomData::Category::TransitionMetal},    // Re - Rhenium
    {190.23f, 0.5f, 4, 0.0f, atomColor(224, 102, 51, 255), 3.20f, 0.20f, AtomData::Category::TransitionMetal},     // Os - Osmium
    {192.22f, 0.5f, 4, 0.0f, atomColor(240, 144, 160, 255), 3.20f, 0.20f, AtomData::Category::TransitionMetal},    // Ir - Iridium
    {195.08f, 0.5f, 4, 0.0f, atomColor(80, 208, 80, 255), 3.20f, 0.20f, AtomData::Category::TransitionMetal},      // Pt - Platinum
    {196.97f, 0.5f, 3, 0.0f, atomColor(200, 128, 51, 255), 3.10f, 0.20f, AtomData::Category::TransitionMetal},     // Au - Gold
    {200.59f, 0.5f, 2, 0.0f, atomColor(125, 128, 176, 255), 3.10f, 0.19f, AtomData::Category::TransitionMetal},    // Hg - Mercury
    {204.38f, 0.5f, 3, 0.0f, atomColor(194, 143, 143, 255), 3.10f, 0.18f, AtomData::Category::PostTransitionMetal}, // Tl - Thallium
    {207.20f, 0.5f, 4, 0.0f, atomColor(194, 143, 143, 255), 3.10f, 0.18f, AtomData::Category::PostTransitionMetal}, // Pb - Lead
    {208.98f, 0.5f, 5, 0.0f, atomColor(194, 143, 143, 255), 3.10f, 0.18f, AtomData::Category::PostTransitionMetal}, // Bi - Bismuth
    {209.00f, 0.5f, 6, 0.0f, atomColor(255, 161, 0, 255), 3.00f, 0.17f, AtomData::Category::PostTransitionMetal},  // Po - Polonium
    {210.00f, 0.5f, 7, 0.0f, atomColor(166, 41, 41, 255), 3.00f, 0.16f, AtomData::Category::Halogen},              // At - Astatine
    {222.00f, 0.5f, 0, 0.0f, atomColor(92, 184, 209, 255), 3.00f, 0.10f, AtomData::Category::NobleGas},           // Rn - Radon

    {223.00f, 0.5f, 1, 0.0f, atomColor(143, 64, 212, 255), 5.30f, 0.22f, AtomData::Category::AlkaliMetal},        // Fr - Francium
    {226.00f, 0.5f, 2, 0.0f, atomColor(61, 255, 0, 255), 5.00f, 0.21f, AtomData::Category::AlkalineEarthMetal},   // Ra - Radium
    {227.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 4.80f, 0.21f, AtomData::Category::Actinide},          // Ac - Actinium
    {232.04f, 0.5f, 4, 0.0f, atomColor(112, 170, 250, 255), 4.70f, 0.21f, AtomData::Category::Actinide},          // Th - Thorium
    {231.04f, 0.5f, 5, 0.0f, atomColor(112, 170, 250, 255), 4.60f, 0.21f, AtomData::Category::Actinide},          // Pa - Protactinium
    {238.03f, 0.5f, 6, 0.0f, atomColor(112, 170, 250, 255), 4.50f, 0.21f, AtomData::Category::Actinide},          // U  - Uranium
    {237.00f, 0.5f, 5, 0.0f, atomColor(112, 170, 250, 255), 4.40f, 0.21f, AtomData::Category::Actinide},          // Np - Neptunium
    {244.00f, 0.5f, 4, 0.0f, atomColor(112, 170, 250, 255), 4.30f, 0.21f, AtomData::Category::Actinide},          // Pu - Plutonium
    {243.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 4.20f, 0.21f, AtomData::Category::Actinide},          // Am - Americium
    {247.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 4.10f, 0.21f, AtomData::Category::Actinide},          // Cm - Curium
    {247.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 4.00f, 0.21f, AtomData::Category::Actinide},          // Bk - Berkelium
    {251.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.90f, 0.21f, AtomData::Category::Actinide},          // Cf - Californium
    {252.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.90f, 0.21f, AtomData::Category::Actinide},          // Es - Einsteinium
    {257.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.80f, 0.21f, AtomData::Category::Actinide},          // Fm - Fermium
    {258.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.80f, 0.21f, AtomData::Category::Actinide},          // Md - Mendelevium
    {259.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.70f, 0.21f, AtomData::Category::Actinide},          // No - Nobelium
    {266.00f, 0.5f, 3, 0.0f, atomColor(112, 170, 250, 255), 3.70f, 0.21f, AtomData::Category::Actinide},          // Lr - Lawrencium
    {267.00f, 0.5f, 4, 0.0f, atomColor(170, 170, 170, 255), 3.60f, 0.21f, AtomData::Category::TransitionMetal},   // Rf - Rutherfordium
    {268.00f, 0.5f, 5, 0.0f, atomColor(170, 170, 170, 255), 3.60f, 0.21f, AtomData::Category::TransitionMetal},   // Db - Dubnium
    {269.00f, 0.5f, 6, 0.0f, atomColor(170, 170, 170, 255), 3.60f, 0.21f, AtomData::Category::TransitionMetal},   // Sg - Seaborgium
    {270.00f, 0.5f, 7, 0.0f, atomColor(170, 170, 170, 255), 3.50f, 0.21f, AtomData::Category::TransitionMetal},   // Bh - Bohrium
    {269.00f, 0.5f, 4, 0.0f, atomColor(170, 170, 170, 255), 3.50f, 0.21f, AtomData::Category::TransitionMetal},   // Hs - Hassium
    {278.00f, 0.5f, 3, 0.0f, atomColor(170, 170, 170, 255), 3.50f, 0.21f, AtomData::Category::TransitionMetal},   // Mt - Meitnerium
    {281.00f, 0.5f, 2, 0.0f, atomColor(170, 170, 170, 255), 3.40f, 0.21f, AtomData::Category::TransitionMetal},   // Ds - Darmstadtium
    {282.00f, 0.5f, 1, 0.0f, atomColor(170, 170, 170, 255), 3.40f, 0.21f, AtomData::Category::TransitionMetal},   // Rg - Roentgenium
    {285.00f, 0.5f, 2, 0.0f, atomColor(170, 170, 170, 255), 3.40f, 0.20f, AtomData::Category::TransitionMetal},   // Cn - Copernicium
    {286.00f, 0.5f, 3, 0.0f, atomColor(170, 170, 170, 255), 3.30f, 0.20f, AtomData::Category::PostTransitionMetal}, // Nh - Nihonium
    {289.00f, 0.5f, 4, 0.0f, atomColor(170, 170, 170, 255), 3.30f, 0.20f, AtomData::Category::PostTransitionMetal}, // Fl - Flerovium
    {290.00f, 0.5f, 5, 0.0f, atomColor(170, 170, 170, 255), 3.20f, 0.20f, AtomData::Category::PostTransitionMetal}, // Mc - Moscovium
    {293.00f, 0.5f, 6, 0.0f, atomColor(170, 170, 170, 255), 3.20f, 0.20f, AtomData::Category::PostTransitionMetal}, // Lv - Livermorium
    {294.00f, 0.5f, 7, 0.0f, atomColor(170, 170, 170, 255), 3.10f, 0.19f, AtomData::Category::Halogen},             // Ts - Tennessine
    {294.00f, 0.5f, 0, 0.0f, atomColor(170, 170, 170, 255), 3.10f, 0.10f, AtomData::Category::NobleGas},           // Og - Oganesson
}};

std::string_view AtomData::symbol(Type type) {
    const size_t index = static_cast<size_t>(type);
    if (index >= kAtomSymbols.size()) {
        return "X";
    }
    return kAtomSymbols[index];
}

std::string_view AtomData::name(Type type) {
    const size_t index = static_cast<size_t>(type);
    if (index >= kAtomNames.size()) {
        return "Unknown";
    }
    return kAtomNames[index];
}

AtomData::Category AtomData::category(Type type) {
    const size_t index = static_cast<size_t>(type);
    if (index >= properties.size()) {
        return Category::Unknown;
    }
    return properties[index].category;
}

std::string_view AtomData::categoryName(Category category) {
    switch (category) {
    case Category::Custom:
        return "Custom";
    case Category::AlkaliMetal:
        return "Alkali metal";
    case Category::AlkalineEarthMetal:
        return "Alkaline earth metal";
    case Category::TransitionMetal:
        return "Transition metal";
    case Category::PostTransitionMetal:
        return "Post-transition metal";
    case Category::Metalloid:
        return "Metalloid";
    case Category::ReactiveNonmetal:
        return "Reactive nonmetal";
    case Category::Halogen:
        return "Halogen";
    case Category::NobleGas:
        return "Noble gas";
    case Category::Lanthanide:
        return "Lanthanide";
    case Category::Actinide:
        return "Actinide";
    case Category::Unknown:
        return "Unknown";
    }
    return "Unknown";
}
