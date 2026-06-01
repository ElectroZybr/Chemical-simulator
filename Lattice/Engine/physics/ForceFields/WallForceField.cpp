#include "WallForceField.h"
#include "Engine/World.h"

#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomStorage.h"

void WallForceField::compute(World& world) const {
    AtomStorage& atoms = world.getAtomStorage();
    const Vec3f& gravity = world.getGravity();
    const Vec3f wallMax = world.getWorldSize() - Vec3f(1.0, 1.0, 1.0);

    for (size_t atomIndex = 0; atomIndex < atoms.mobileCount(); ++atomIndex) {
        float forceX = atoms.forceX(atomIndex);
        float forceY = atoms.forceY(atomIndex);
        float forceZ = atoms.forceZ(atomIndex);

        // мягкие стены
        softWalls(atoms.posX(atomIndex), atoms.posY(atomIndex), atoms.posZ(atomIndex), forceX, forceY, forceZ, wallMax);
        // постоянная сила
        applyGravityForce(forceX, forceY, forceZ, gravity);

        atoms.forceX(atomIndex) = forceX;
        atoms.forceY(atomIndex) = forceY;
        atoms.forceZ(atomIndex) = forceZ;
    }
}

void WallForceField::applyWall(float coord, float& force, float max) {
    constexpr float k = 500.0f;
    constexpr float border = 2.0f;

    const float penLow = border - coord;
    const float penHigh = coord - (max - border);

    const float fLow = penLow > 0.0f ? penLow * penLow * penLow * penLow * penLow * penLow * k : 0.0f;
    const float fHigh = penHigh > 0.0f ? penHigh * penHigh * penHigh * penHigh * penHigh * penHigh * k : 0.0f;

    force += fLow - fHigh;
}

void WallForceField::softWalls(float coordX, float coordY, float coordZ, float& forceX, float& forceY, float& forceZ,
                               const Vec3f& wallMax) const {
    applyWall(coordX, forceX, wallMax.x);
    applyWall(coordY, forceY, wallMax.y);
    applyWall(coordZ, forceZ, wallMax.z);
}

void WallForceField::applyGravityForce(float& forceX, float& forceY, float& forceZ, const Vec3f& gravity) {
    forceX += gravity.x;
    forceY += gravity.y;
    forceZ += gravity.z;
}
