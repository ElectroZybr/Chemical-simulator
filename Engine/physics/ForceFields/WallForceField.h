#pragma once

#include "Engine/World.h"
#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomStorage.h"

class WallForceField {
public:
    void syncWalls(const World& box);
    void compute(AtomStorage& atoms, const Vec3f& gravity) const;

private:
    static void applyWall(float coord, float& force, float max);
    void softWalls(float coordX, float coordY, float coordZ, float& forceX, float& forceY, float& forceZ) const;
    static void applyGravityForce(float& forceX, float& forceY, float& forceZ, const Vec3f& gravity);

    float wallMaxX_ = 0.0f;
    float wallMaxY_ = 0.0f;
    float wallMaxZ_ = 0.0f;
};
