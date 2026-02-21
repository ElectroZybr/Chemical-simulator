#pragma once

#include <SFML/Graphics/Color.hpp>
#include <array>
#include "..\math\Vec2D.h"
#include "..\math\Vec3D.h"
#include "Bond.h"
#include "BondTable.h"
#include "SpatialGrid.h"
#include <vector>
#include <list>


// Общие данные для всех атомов одного типа
struct StaticAtomicData {
    const double mass;
    const double radius;
    const char maxValence;
    const double defaultCharge;
    const sf::Color color;
};


class Atom {
private:
    static SpatialGrid* grid;
    static const std::array<StaticAtomicData, 118> properties;
public:
    Vec3D coords;
    Vec3D speed;
    Vec3D acceleration;
    Vec3D PrevAcceleration;

    int type;
    int valence;
    float r0 = 2.5;
    float De = 0.2;
    float a = 1.0;

    bool isFixed = false;
    bool isSelect = false;
    std::vector<Atom*> bonds;

    Atom (Vec3D start_coords, Vec3D start_speed, int type, bool fixed = false);

    void PredictPosition(double deltaTime);
    void Bounce();
    void SoftWalls(double deltaTime);
    void ComputeForces(double deltaTime);

    float MorseForce(float distanse);
    float MorsePotential(float distanse);
    Vec3D Force(Atom *a1, Atom *a2, double dt);
    void CorrectVelosity(double dt);
    void Euler(double deltaTime);
    void Verlet(double deltaTime);

    float kineticEnergy() const;
    float pairPotentialEnergy(Atom* other);

    static void setGrid(SpatialGrid* grid);

    const StaticAtomicData& getProps() const {
        return properties.at(type);
    }
};