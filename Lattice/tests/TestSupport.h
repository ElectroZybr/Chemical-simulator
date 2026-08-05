#pragma once

#include <cmath>
#include <iostream>

#include <glm/vec3.hpp>

inline bool isClose(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

inline bool isClose(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    return isClose(a.x, b.x, eps) && isClose(a.y, b.y, eps) && isClose(a.z, b.z, eps);
}

inline void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "TEST FAILED: " << message << std::endl;
        std::terminate();
    }
}