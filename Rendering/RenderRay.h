#pragma once

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

class RenderRay final {
public:
    glm::vec3 origin;
    glm::vec3 dir;

    explicit RenderRay(const glm::vec3& origin, const glm::vec3& dir) : origin(origin), dir(dir) {}

    [[nodiscard]] glm::vec3 at(float t) const { return origin + dir * t; }
};

struct RenderRaySphereHit {
    float t = 0.0f;
    glm::vec3 point;
};

inline bool renderRaySphereIntersect(const RenderRay& ray, const glm::vec3& center, float radius, RenderRaySphereHit& hit) {
    const glm::vec3 oc = ray.origin - center;

    constexpr float a = 1.0f;
    const float b = 2.0f * glm::dot(oc, ray.dir);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float d = b * b - 4.0f * a * c;

    if (d < 0.0f) {
        return false;
    }

    float t = (-b - std::sqrt(d)) / (2.0f * a);
    if (t < 0.0f) {
        t = (-b + std::sqrt(d)) / (2.0f * a);
    }

    if (t < 0.0f) {
        return false;
    }

    hit.t = t;
    hit.point = ray.at(t);
    return true;
}