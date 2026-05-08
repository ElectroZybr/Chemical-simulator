#pragma once

#include "Engine/World.h"
#include "Engine/math/Vec3.h"

class WallForceField {
public:
    void compute(World& world) const;

private:
    static void applyWall(float coord, float& force, float max);
    void softWalls(float coordX, float coordY, float coordZ, float& forceX, float& forceY, float& forceZ, const Vec3f& wallMax) const;
    static void applyGravityForce(float& forceX, float& forceY, float& forceZ, const Vec3f& gravity);
};
