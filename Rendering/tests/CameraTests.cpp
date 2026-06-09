#include <cmath>
#include <cstdlib>
#include <iostream>

#include <glm/common.hpp>

#include "Rendering/RenderRay.h"
#include "Rendering/camera/Camera.h"

namespace {
    bool almostEqual(float a, float b, float epsilon = 1e-3f) {
        return std::fabs(a - b) <= epsilon;
    }

    bool vec3AlmostEqual(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 1e-3f) {
        return almostEqual(lhs.x, rhs.x, epsilon) && almostEqual(lhs.y, rhs.y, epsilon) && almostEqual(lhs.z, rhs.z, epsilon);
    }

    int testCameraCentersOnSceneOffset() {
        Camera camera;
        camera.setMode(Camera::Mode::Orbit);
        camera.setSceneBounds(glm::vec3(20.0f, 40.0f, 60.0f), glm::vec3(5.0f, 10.0f, 15.0f));
        camera.setScreenSize(glm::vec2(1280.0f, 720.0f));
        camera.resetView();

        const glm::vec3 expectedCenter(15.0f, 30.0f, 45.0f);
        const RenderRay centerRay = camera.screenToRay(640.0f, 360.0f);
        const float distanceToCenter = glm::length(expectedCenter - camera.getEyePosition());
        if (!vec3AlmostEqual(centerRay.at(distanceToCenter), expectedCenter)) {
            std::cerr << "Camera center ray does not pass through scene center\n";
            return EXIT_FAILURE;
        }

        if (!vec3AlmostEqual(camera.getForwardVector(), glm::vec3(0.0f, 0.0f, -1.0f))) {
            std::cerr << "Unexpected default forward vector\n";
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    int testRenderMathSphereIntersection() {
        const RenderRay ray(glm::vec3(0.0f, 0.0f, -5.0f), glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f)));
        RenderRaySphereHit hit{};

        if (!renderRaySphereIntersect(ray, glm::vec3(0.0f), 1.0f, hit)) {
            std::cerr << "Expected ray/sphere intersection\n";
            return EXIT_FAILURE;
        }

        if (!almostEqual(hit.t, 4.0f)) {
            std::cerr << "Unexpected intersection distance: " << hit.t << "\n";
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
}

int main() {
    if (testCameraCentersOnSceneOffset() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (testRenderMathSphereIntersection() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
