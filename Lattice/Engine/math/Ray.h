#pragma once

#include "Engine/math/Vec3.h"

class Ray final {
public:
    Vec3f origin;
    Vec3f dir;

    explicit Ray(const Vec3f& origin, const Vec3f& dir) : origin(origin), dir(dir) {}

    // Точка на луче на расстоянии t
    [[nodiscard]] Vec3f at(float t) const { return origin + dir * t; }
};

// Результат пересечения луча со сферой
struct RaySphereHit {
    float t;     // расстояние от origin до точки пересечения
    Vec3f point; // точка пересечения в мировых координатах
};

// Возвращает false если нет пересечения или сфера позади камеры
inline bool raySphereIntersect(const Ray& ray, const Vec3f& center, float radius, RaySphereHit& hit) {
    const Vec3f oc(ray.origin.x - center.x, ray.origin.y - center.y, ray.origin.z - center.z);

    constexpr float a = 1.f; // ray.dir.sqrAbs(); ray.dir всегда нормализован
    const float b = 2.0 * oc.dot(ray.dir);
    const float c = oc.sqrAbs() - radius * radius;
    const float D = b * b - 4.0 * a * c;

    if (D < 0.0) {
        return false;
    }

    float t = (-b - std::sqrt(D)) / (2.0 * a);
    if (t < 0.0) {
        t = (-b + std::sqrt(D)) / (2.0 * a);
    }

    if (t < 0.0) {
        return false;
    }

    hit.t = t;
    hit.point = ray.at(t);
    return true;
}
