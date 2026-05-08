#include "WallForceField.h"

void WallForceField::syncWalls(const World& world) {
    wallMaxX_ = world.getWorldSize().x - 1.0f;
    wallMaxY_ = world.getWorldSize().y - 1.0f;
    wallMaxZ_ = world.getWorldSize().z - 1.0f;
}

void WallForceField::compute(AtomStorage& atoms, const Vec3f& gravity) const {
    for (size_t atomIndex = 0; atomIndex < atoms.mobileCount(); ++atomIndex) {
        float forceX = atoms.forceX(atomIndex);
        float forceY = atoms.forceY(atomIndex);
        float forceZ = atoms.forceZ(atomIndex);

        // мягкие стены
        softWalls(atoms.posX(atomIndex), atoms.posY(atomIndex), atoms.posZ(atomIndex), forceX, forceY, forceZ);
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

void WallForceField::softWalls(float coordX, float coordY, float coordZ, float& forceX, float& forceY, float& forceZ) const {
    applyWall(coordX, forceX, wallMaxX_);
    applyWall(coordY, forceY, wallMaxY_);
    applyWall(coordZ, forceZ, wallMaxZ_);
}

void WallForceField::applyGravityForce(float& forceX, float& forceY, float& forceZ, const Vec3f& gravity) {
    forceX += gravity.x;
    forceY += gravity.y;
    forceZ += gravity.z;
}
