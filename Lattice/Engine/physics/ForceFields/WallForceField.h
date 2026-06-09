#pragma once

#include <glm/glm.hpp>

class World;

class WallForceField {
public:
    void compute(World& world) const;

private:
    static void applyWall(float coord, float& force, float max);
    void softWalls(float coordX, float coordY, float coordZ, float& forceX, float& forceY, float& forceZ, const glm::vec3& wallMax) const;
    static void applyGravityForce(float& forceX, float& forceY, float& forceZ, const glm::vec3& gravity);
};
