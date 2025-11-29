#include <cmath>

#include "Bond.h"
#include "Atom.h"

BondTable Bond::bond_default_props;
std::list<Bond> Bond::bonds_list;


Bond::Bond (Atom* _a, Atom* _b) : a(_a), b(_b) {//, float _r0, float _k, float _D_e, float _alpha
    BondParams bond_params = bond_default_props.get(AtomType(_a->type), AtomType(_b->type));
    std::cout << "<Bond params> a:" << _a->type << " b: "<< _b->type << " r0: "<< bond_params.r0 << " k: "<< bond_params.k << " De: "<< bond_params.D_e << std::endl;
    params.r0 = bond_params.r0;
    params.k = bond_params.k;
    params.D_e = bond_params.D_e;
    params.alpha = bond_params.alpha;
}

void Bond::forceBond(double dt) {
    Vec3D delta = a->coords - b->coords;
    float distanse = sqrt(delta.dot(delta));
    if (distanse > 3) {
        std::cout << "<Break bond>" << std::endl;
        Bond::BreakBond(this);
        return;
    }
    Vec3D unit = (a->coords - b->coords) / distanse;
    Vec3D force = unit * MorseForce(distanse) * 100;
    // std::cout << "<Force bond2> " << ((a->acceleration - force / a->getProps().mass) * 1).x << std::endl;
    a->acceleration = (a->acceleration - force / a->getProps().mass);
    b->acceleration = (b->acceleration + force / b->getProps().mass);
}

float Bond::MorseForce(float distanse) {
    float exponent = std::exp(-params.k * (distanse - params.r0));
    return 2 * params.D_e * params.alpha * (1 - exponent) * exponent;
}

Bond* Bond::CreateBond(Atom* a, Atom* b) {
    bonds_list.emplace_back(a, b);
    auto it = std::prev(bonds_list.end());
    // bonds_list.push_back(Bond(a, b));
    it->self_it = it;
    // a->bonds.push_back(&(*it));
    // b->bonds.push_back(&(*it));
    a->valence--;
    b->valence--;
    return &(*it);
}

void Bond::BreakBond(Bond* bond) {
    // {
    // auto& v = bond->a->bonds;
    // v.erase(std::remove(v.begin(), v.end(), bond), v.end());
    // }
    // bond->b->bonds.erase(&(*it));
    bond->a->valence++;
    bond->b->valence++;

    bonds_list.erase(bond->self_it);
}