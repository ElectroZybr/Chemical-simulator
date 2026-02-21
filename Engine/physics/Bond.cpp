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
    // params.alpha = bond_params.alpha;
}

void Bond::forceBond(double dt) {
    Vec3D delta = a->coords - b->coords;
    float distanse = sqrt(delta.dot(delta));
    if (distanse > 3) {
        Bond::BreakBond(this);
        return;
    }
    Vec3D unit = (a->coords - b->coords) / distanse;
    Vec3D force = unit * MorseForce(distanse);

    a->acceleration = (a->acceleration + force / a->getProps().mass);
    b->acceleration = (b->acceleration - force / b->getProps().mass);
}

float Bond::MorseForce(float distanse) {
    float exp_a = std::exp(-params.a * (distanse - params.r0));
    return 2 * params.De * params.a * (exp_a * exp_a - exp_a);
}

void Bond::angleForce(Atom* o, Atom* b, Atom* c) {
    // Атом o - центральный, b и c - присоединенные
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
    Vec3D force_o = -(force_b + force_c);

    b->acceleration = (b->acceleration + force_b / b->getProps().mass);
    c->acceleration = (c->acceleration + force_c / c->getProps().mass);
    o->acceleration = (o->acceleration + force_o / o->getProps().mass);

    // std::cout
    //         // << "X " << force_b.x
    //         // << " | Y " << force_b.y
    //         << " | d " << theta_0
    //         << " | Loss " << angle_loss * 180.0 / M_PI
    //         << " | Angle " << angle_theta * 180.0 / M_PI
    //         << std::endl;
    // float distance_sq = delta.dot(delta);
}

Bond* Bond::CreateBond(Atom* a, Atom* b) {
    std::cout << "<Create bond>" << std::endl;
    bonds_list.emplace_back(a, b);
    auto it = std::prev(bonds_list.end());
    // bonds_list.push_back(Bond(a, b));
    it->self_it = it;
    a->bonds.push_back(b);
    b->bonds.push_back(a);
    // std::cout << "<a>" << a->bonds[0] << std::endl;
    // std::cout << "<a>" << a->bonds.size() << std::endl;
    // std::cout << "<b>" << b->bonds.size() << std::endl;
    a->valence--;
    b->valence--;
    return &(*it);
}

void Bond::BreakBond(Bond* bond) {
    std::cout << "<Break bond>" << std::endl;
    std::vector<Atom*>* bonds = &bond->a->bonds;
    bonds->erase(std::remove(bonds->begin(), bonds->end(), bond->b), bonds->end());
    bonds = &bond->b->bonds;
    bonds->erase(std::remove(bonds->begin(), bonds->end(), bond->a), bonds->end());
    // std::cout << bond->a->bonds.size() << std::endl;
    // std::cout << "<a>" << bond->a->bonds[0] << std::endl;

    bond->a->valence++;
    bond->b->valence++;

    bonds_list.erase(bond->self_it);
}