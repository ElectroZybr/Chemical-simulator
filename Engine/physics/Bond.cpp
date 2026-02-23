#include <cmath>
#include <algorithm>

#include "Bond.h"
#include "Atom.h"

BondTable Bond::bond_default_props;
std::list<Bond> Bond::bonds_list;


Bond::Bond (Atom* _a, Atom* _b) : a(_a), b(_b) {//, float _r0, float _k, float _D_e, float _alpha
    BondParams bond_params = bond_default_props.get(AtomType(_a->type), AtomType(_b->type));
    std::cout << "<Bond params> a:" << _a->type << " b: "<< _b->type << " r0: "<< bond_params.r0 << " a: "<< bond_params.a << " De: "<< bond_params.De << std::endl;
    params.r0 = bond_params.r0;
    params.a = bond_params.a;
    params.De = bond_params.De;
}

void Bond::forceBond(double dt) {
    Vec3D delta = a->coords - b->coords;
    Vec3D hat = delta.normalized();
    float len = delta.length();
    Vec3D force = hat * MorseForce(len);

    a->force += force;
    b->force -= force;
}

bool Bond::shouldBreak() const {
    Vec3D delta = a->coords - b->coords;
    float distanse = sqrt(delta.dot(delta));
    return distanse > 3;
}

float Bond::MorseForce(float distanse) {
    /* производная потенциала Морзе по расстоянию */
    float exp_a = std::exp(-params.a * (distanse - params.r0));
    return 2 * params.De * params.a * (exp_a * exp_a - exp_a);
}

void Bond::angleForce(Atom* o, Atom* b, Atom* c) {
    /* Атом o - центральный, b и c - присоединенные */
    Vec3D delta_ob = b->coords - o->coords; // Вектор направления ob
    Vec3D delta_oc = c->coords - o->coords; // Вектор направления oc

    float len_ob = delta_ob.length(); // Скаляр вектора ob
    float len_oc = delta_oc.length(); // Скаляр вектора oc

    Vec3D ob_hat = delta_ob.normalized(); // нормализованный вектор направления ob
    Vec3D oc_hat = delta_oc.normalized(); // нормализованный вектор направления oc

    double cos_theta = ob_hat.dot(oc_hat); // косинус угла theta
    double sin_theta = std::sqrt(1-cos_theta*cos_theta); // синус угла theta
    double angle_theta = std::acos(cos_theta); // Угол theta в радианах
    double theta_0 = 60.f / 180.f * M_PI; // Заданный угол theta в градусах

    double angle_loss = angle_theta - theta_0; // Текущая ошибка угла

    double k = 50;
    
    if (sin_theta < 1e-6) return;

    Vec3D force_b = -((oc_hat - ob_hat * cos_theta) / len_ob) * (-k * angle_loss / sin_theta); // градиент сил b
    Vec3D force_c = -((ob_hat - oc_hat * cos_theta) / len_oc) * (-k * angle_loss / sin_theta); // градиент сил c

    b->force += force_b;
    c->force += force_c;
    Vec3D force_o = -(force_b + force_c);
}

Bond* Bond::CreateBond(Atom* a, Atom* b) {
    std::cout << "<Create bond>" << std::endl;
    bonds_list.emplace_back(a, b);
    auto it = std::prev(bonds_list.end());
    a->bonds.push_back(b);
    b->bonds.push_back(a);

    a->valence--;
    b->valence--;
    return &(*it);
}

void Bond::detach() {
    std::vector<Atom*>* bonds = &a->bonds;
    bonds->erase(std::remove(bonds->begin(), bonds->end(), b), bonds->end());
    bonds = &b->bonds;
    bonds->erase(std::remove(bonds->begin(), bonds->end(), a), bonds->end());

    a->valence++;
    b->valence++;
}

void Bond::BreakBond(Bond* bond) {
    if (!bond) return;
    std::cout << "<Break bond>" << std::endl;
    bond->detach();

    for (auto it = bonds_list.begin(); it != bonds_list.end(); ++it) {
        if (&(*it) == bond) {
            bonds_list.erase(it);
            return;
        }
    }
}
