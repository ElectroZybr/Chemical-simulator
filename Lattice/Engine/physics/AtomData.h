#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

union Color {
    uint32_t rgba;
    struct {
        uint8_t r, g, b, a;
    };
};

struct StaticAtomicData {
    const float mass;
    const float radius;
    const char maxValence;
    const float defaultCharge;
    const Color color;
    const float ljA0;
    const float ljEps;
};

class AtomData {
public:
    // clang-format off
    enum class Type : uint8_t {
        Z,

        H, He,

        Li, Be, B, C, N, O, F, Ne,

        Na, Mg, Al, Si, P, S, Cl, Ar, 

        K, Ca, Sc, Ti, V, Cr, Mn, Fe,
        Co, Ni, Cu, Zn, Ga, Ge, As, Se, Br, Kr,

        Rb, Sr, Y, Zr, Nb, Mo, Tc, Ru, 
        Rh, Pd, Ag, Cd, In, Sn, Sb, Te, I, Xe,

        Cs, Ba, La, Ce, Pr, Nd, Pm, Sm,
        Eu, Gd, Tb, Dy, Ho, Er, Tm, Yb,
        Lu, Hf, Ta, W, Re, Os, Ir, Pt, 
        Au, Hg, Tl, Pb, Bi, Po, At, Rn,

        Fr, Ra, Ac, Th, Pa, U, Np, Pu,
        Am, Cm, Bk, Cf, Es, Fm, Md, No,
        Lr, Rf, Db, Sg, Bh, Hs, Mt, Ds,
        Rg, Cn, Nh, Fl, Mc, Lv, Ts, Og,
        COUNT
    };
    // clang-format on

private:
    static const std::array<StaticAtomicData, static_cast<size_t>(Type::COUNT)> properties;

public:
    static const StaticAtomicData& getProps(Type type) { return properties.at(static_cast<int>(type)); }
    static std::string_view symbol(Type type);
};
