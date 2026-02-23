#pragma once

#include "..\math\Vec3D.h"
#include "BondTable.h"
#include <list>

class Atom;


class Bond {
private:
public:
    static BondTable bond_default_props;
    
    static Bond* CreateBond(Atom* a, Atom* b);
    static void BreakBond(Bond* bond);
    static std::list<Bond> bonds_list;
    static void angleForce(Atom* a, Atom* b, Atom* c);

    Bond (Atom* a, Atom* b);//, float r0, float k, float D_e, float alpha

    void forceBond(double dt);
    bool shouldBreak() const;
    void detach();
    float MorseForce(float distanse);
    // void angleForce(Atom* a, Atom* b, Atom* c);

    Atom* a;
    Atom* b;

    BondParams params;
    // float r0;
    // float k;
    // float D_e;
    // float alpha;
};
