#include "Camera.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace {
    float wrapRadians(float angle) {
        constexpr float pi = glm::pi<float>();
        constexpr float twoPi = 2.0f * pi;

        angle = std::fmod(angle + pi, twoPi);
        if (angle < 0.0f) {
            angle += twoPi;
        }
        return angle - pi;
    }

    float smoothStep01(float t) { return t * t * (3.0f - 2.0f * t); }

    glm::vec3 orbitUpVector(float azimuth, float elevation) {
        return glm::normalize(glm::vec3(-std::sin(elevation) * std::sin(azimuth), std::cos(elevation),
                                        -std::sin(elevation) * std::cos(azimuth)));
    }

    glm::vec3 rotateAroundAxis(glm::vec3 v, glm::vec3 axis, float angle) {
        axis = glm::normalize(axis);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.0f - c);
    }

    float defaultOrbitDistanceForScene(const glm::vec3& sceneSize, float orbitFovDegrees) {
        const float maxSide = std::max({sceneSize.x, sceneSize.y, sceneSize.z});
        return (maxSide * 0.5f * 1.1f) / std::tan(glm::radians(orbitFovDegrees) * 0.5f);
    }

    glm::vec3 orbitOffsetDirection(float azimuth, float elevation) {
        return glm::vec3(std::cos(elevation) * std::sin(azimuth), std::sin(elevation), std::cos(elevation) * std::cos(azimuth));
    }

    glm::vec3 projectOntoPlane(glm::vec3 v, glm::vec3 normal) { return v - normal * glm::dot(v, normal); }

    glm::vec3 stabilizedOrbitUp(glm::vec3 direction, glm::vec3 currentUp) {
        glm::vec3 desiredUp = projectOntoPlane(currentUp, direction);
        if (glm::dot(desiredUp, desiredUp) <= 1e-6f) {
            const glm::vec3 preferredUp = std::abs(direction.y) > 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            desiredUp = projectOntoPlane(preferredUp, direction);
        }
        if (glm::dot(desiredUp, desiredUp) <= 1e-6f) {
            desiredUp = projectOntoPlane(glm::vec3(1.0f, 0.0f, 0.0f), direction);
        }

        desiredUp = glm::normalize(desiredUp);
        const std::array<glm::vec3, 6> candidateAxes = {
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
        };

        float bestDot = -std::numeric_limits<float>::infinity();
        glm::vec3 bestUp(0.0f);
        for (const glm::vec3& axis : candidateAxes) {
            glm::vec3 candidateUp = projectOntoPlane(axis, direction);
            if (glm::dot(candidateUp, candidateUp) <= 1e-6f) {
                continue;
            }
            candidateUp = glm::normalize(candidateUp);
            const float alignment = glm::dot(candidateUp, desiredUp);
            if (alignment > bestDot) {
                bestDot = alignment;
                bestUp = candidateUp;
            }
        }

        if (glm::dot(bestUp, bestUp) <= 1e-6f) {
            const glm::vec3 fallbackUp = projectOntoPlane(glm::vec3(1.0f, 0.0f, 0.0f), direction);
            return glm::normalize(fallbackUp);
        }
        return bestUp;
    }
}

Camera::Camera(float moveSpeed, float zoomSpeed) : moveSpeed(moveSpeed), zoomSpeed(zoomSpeed), isDragging(false), lastMousePos(0, 0) {}

void Camera::resetView() {
    orbitAnimationActive = false;
    azimuth = 0.f;
    elevation = 0.f;
    orbitUp = glm::vec3(0.f, 1.f, 0.f);

    const float distance = defaultOrbitDistanceForScene(sceneSize, kOrbitFovDegrees);
    orbitCenter = sceneOffset + sceneSize * 0.5f;

    if (mode == Camera::Mode::Mode2D) {
        position = glm::vec2(orbitCenter.x, orbitCenter.y);
        constexpr float margin = 0.85f;

        const float zoomX = (screenSize.x * margin) / sceneSize.x;
        const float zoomY = (screenSize.y * margin) / sceneSize.y;

        setZoom(std::min(zoomX, zoomY));
    }
    else {
        position = glm::vec2(orbitCenter);
        const float safeDistance = std::max(distance, 1e-3f);
        setZoom(moveSpeed / safeDistance);
        const glm::vec3 orbitOffset(std::cos(elevation) * std::sin(azimuth), std::sin(elevation), std::cos(elevation) * std::cos(azimuth));
        freePosition = orbitCenter + orbitOffset * safeDistance;
    }
}

void Camera::move(glm::vec2 offset) { setPosition(getPosition() + offset); }

void Camera::setPosition(glm::vec2 pos) {
    position = pos;
    if (mode != Mode::Mode2D) {
        return;
    }

    const glm::vec3 forward = -getForwardVector();
    const float depth = glm::dot(orbitCenter, forward);
    orbitCenter = getPlanarRightAxis() * pos.x + getPlanarUpAxis() * pos.y + forward * depth;
}

glm::vec2 Camera::getPosition() const {
    if (mode != Mode::Mode2D) {
        return position;
    }
    return glm::vec2(glm::dot(orbitCenter, getPlanarRightAxis()), glm::dot(orbitCenter, getPlanarUpAxis()));
}

void Camera::setZoom(float new_zoom) {
    zoom = std::clamp(new_zoom, 0.1f, 10000.f);
    speed = moveSpeed / zoom;
}

void Camera::setMode(Mode newMode) {
    orbitAnimationActive = false;
    if (mode == newMode) {
        return;
    }

    if (mode == Mode::Orbit && newMode == Mode::Free) {
        const glm::vec3 eye = getEyePosition();
        freePosition = glm::vec3(eye.x, eye.y, eye.z);
    }
    else if (mode == Mode::Free && newMode == Mode::Orbit) {
        const glm::vec3 eye = freePosition;
        orbitCenter = sceneOffset + sceneSize * 0.5f;

        const glm::vec3 toEye = eye - orbitCenter;
        const float distance = glm::length(toEye);
        if (distance > 1e-6f) {
            setZoom(moveSpeed / distance);
            azimuth = std::atan2(toEye.x, toEye.z);
            elevation = std::asin(std::clamp(toEye.y / distance, -1.0f, 1.0f));
            orbitUp = orbitUpVector(azimuth, elevation);
        }
    }

    mode = newMode;
}

void Camera::zoomAt(float factor, glm::vec2 mousePos) {
    orbitAnimationActive = false;
    // Изменяем уровень зума с учетом направления к курсору
    zoom *= (1.f + factor * zoomSpeed);
    zoom = std::clamp(zoom, 1.f, 500.f);
    speed = moveSpeed / zoom;

    // Плавное следование за указателем мыши при зуме
    if (zoom > 1.f && zoom < 500.f) {
        glm::vec2 deltaPos(mousePos - screenSize * 0.5f);
        deltaPos.y *= -1.f;
        setPosition(getPosition() + deltaPos * 0.1f / zoom * factor);
    }
}

void Camera::orbitDrag(glm::ivec2 delta) {
    constexpr float sensitivity = 0.005f;
    orbitRotate(-delta.x * sensitivity, -delta.y * sensitivity);
}

void Camera::orbitRotate(float azimuthDelta, float elevationDelta) {
    orbitAnimationActive = false;
    const glm::vec3 center(orbitCenter.x, orbitCenter.y, orbitCenter.z);
    const glm::vec3 eye = getEyePosition();
    glm::vec3 offset = eye - center;

    const float distance = glm::length(offset);
    if (distance <= 1e-6f) {
        return;
    }

    const glm::vec3 up = glm::normalize(orbitUp);

    offset = rotateAroundAxis(offset, up, azimuthDelta);
    const glm::vec3 forwardAfterYaw = glm::normalize(-offset);
    const glm::vec3 right = glm::normalize(glm::cross(forwardAfterYaw, up));

    offset = rotateAroundAxis(offset, right, elevationDelta);
    orbitUp = rotateAroundAxis(up, right, elevationDelta);
    offset = glm::normalize(offset) * distance;
    orbitUp = glm::normalize(orbitUp - glm::normalize(offset) * glm::dot(orbitUp, glm::normalize(offset)));

    azimuth = wrapRadians(std::atan2(offset.x, offset.z));
    elevation = wrapRadians(std::asin(std::clamp(offset.y / distance, -1.0f, 1.0f)));
}

void Camera::freeDrag(glm::ivec2 delta) {
    orbitAnimationActive = false;
    constexpr float sensitivity = 0.003f;
    azimuth -= delta.x * sensitivity;
    elevation += delta.y * sensitivity;
    elevation = std::clamp(elevation, -1.5f, 1.5f);
}

// Для 3д режимов возвращает cameraPos + cameraDir * 10
glm::vec3 Camera::screenToWorld(glm::ivec2 screenPos) const {
    if (mode == Mode::Mode2D) {
        const RenderRay ray = screenToRay(static_cast<float>(screenPos.x), static_cast<float>(screenPos.y));
        const glm::vec3 planeNormal = -getForwardVector();
        const float denominator = glm::dot(ray.dir, planeNormal);
        if (std::abs(denominator) <= 1e-6f) {
            return orbitCenter;
        }
        const float t = glm::dot(orbitCenter - ray.origin, planeNormal) / denominator;
        return ray.at(t);
    }

    const RenderRay ray = screenToRay(screenPos.x, screenPos.y);
    return ray.at(100.0);
}

glm::ivec2 Camera::worldToScreen(glm::vec3 worldPos) const {
    const glm::vec4 clip = getProjectionMatrix() * getViewMatrix() * glm::vec4(worldPos.x, worldPos.y, worldPos.z, 1.f);
    if (mode != Mode::Mode2D && clip.w <= 0.f) {
        return {0, 0};
    }

    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;

    return glm::ivec2(static_cast<int>((ndcX + 1.f) * 0.5f * screenSize.x), static_cast<int>((-ndcY + 1.f) * 0.5f * screenSize.y));
}

glm::vec3 Camera::getEyePosition() const {
    if (mode == Mode::Free) {
        return glm::vec3(freePosition.x, freePosition.y, freePosition.z);
    }

    const glm::vec3 glmCenter(orbitCenter.x, orbitCenter.y, orbitCenter.z);
    const float r = (mode == Mode::Mode2D) ? defaultOrbitDistanceForScene(sceneSize, kOrbitFovDegrees) : (moveSpeed / zoom);
    return glmCenter + r * orbitOffsetDirection(azimuth, elevation);
}

glm::vec3 Camera::getForwardVector() const {
    return glm::normalize(-orbitOffsetDirection(azimuth, elevation));
}

glm::mat4 Camera::getViewMatrix() const {
    if (mode == Mode::Free) {
        const glm::vec3 forward = getForwardVector();
        const glm::vec3 eye(freePosition.x, freePosition.y, freePosition.z);
        return glm::lookAt(eye, eye + forward, glm::vec3(0.f, 1.f, 0.f));
    }

    // Orbit camera keeps its target stable until resetView().
    glm::vec3 center = orbitCenter;
    glm::vec3 eye = getEyePosition();
    return glm::lookAt(eye, glm::vec3(center.x, center.y, center.z), orbitUp);
}

glm::mat4 Camera::getProjectionMatrix() const {
    if (mode == Mode::Mode2D) {
        const float aspect = static_cast<float>(screenSize.x) / static_cast<float>(screenSize.y);
        const float viewWidth = static_cast<float>(screenSize.x) / getZoom();
        const float viewHeight = viewWidth / aspect;
        return glm::orthoRH_ZO(-viewWidth / 2.f, viewWidth / 2.f, -viewHeight / 2.f, viewHeight / 2.f, -10000.f, 10000.f);
    }
    const float fovDegrees = (mode == Mode::Free) ? kFreeFovDegrees : kOrbitFovDegrees;
    return glm::perspective(glm::radians(fovDegrees), screenSize.x / screenSize.y, kNearPlane, kFarPlane);
}

void Camera::setOrthographicView(glm::vec3 direction, glm::vec3 up) {
    if (glm::dot(direction, direction) <= 1e-8f) {
        return;
    }

    direction = glm::normalize(direction);
    orbitUp = stabilizedOrbitUp(direction, up);
    const glm::vec3 offsetDirection = -direction;
    azimuth = wrapRadians(std::atan2(offsetDirection.x, offsetDirection.z));
    elevation = wrapRadians(std::asin(std::clamp(offsetDirection.y, -1.0f, 1.0f)));
    orbitCenter = sceneOffset + sceneSize * 0.5f;
    position = glm::vec2(glm::dot(orbitCenter, getPlanarRightAxis()), glm::dot(orbitCenter, getPlanarUpAxis()));
}

void Camera::update(float deltaTime) {
    if (!orbitAnimationActive || mode == Mode::Free) {
        return;
    }

    orbitAnimationT = std::min(orbitAnimationT + std::max(deltaTime, 0.0f), orbitAnimationDuration);
    const float rawT = orbitAnimationDuration > 1e-6f ? (orbitAnimationT / orbitAnimationDuration) : 1.0f;
    const float t = smoothStep01(std::clamp(rawT, 0.0f, 1.0f));

    glm::vec3 direction = glm::mix(orbitAnimationStartDirection, orbitAnimationTargetDirection, t);
    if (glm::dot(direction, direction) <= 1e-8f) {
        direction = orbitAnimationTargetDirection;
    }
    direction = glm::normalize(direction);
    azimuth = wrapRadians(std::atan2(direction.x, direction.z));
    elevation = wrapRadians(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));

    orbitUp = orbitAnimationStartUp + (orbitAnimationTargetUp - orbitAnimationStartUp) * t;
    orbitUp = orbitUp - direction * glm::dot(orbitUp, direction);
    if (glm::dot(orbitUp, orbitUp) <= 1e-8f) {
        orbitUp = orbitAnimationTargetUp;
    }
    orbitUp = glm::normalize(orbitUp);
    setZoom(orbitAnimationStartZoom + (orbitAnimationTargetZoom - orbitAnimationStartZoom) * t);

    if (rawT >= 1.0f) {
        azimuth = orbitAnimationTargetAzimuth;
        elevation = orbitAnimationTargetElevation;
        orbitUp = orbitAnimationTargetUp;
        setZoom(orbitAnimationTargetZoom);
        orbitAnimationActive = false;
    }
}

void Camera::snapToDirection(glm::vec3 direction) {
    if (glm::dot(direction, direction) <= 1e-8f) {
        return;
    }

    direction = glm::normalize(direction);
    const bool keepOrthographic = mode == Mode::Mode2D;
    if (mode == Mode::Free) {
        setMode(Mode::Orbit);
    }

    orbitCenter = sceneOffset + sceneSize * 0.5f;
    const glm::vec3 currentOffset = getEyePosition() - orbitCenter;
    float distance = glm::length(currentOffset);
    if (distance <= 1e-6f) {
        distance = defaultOrbitDistanceForScene(sceneSize, kOrbitFovDegrees);
    }

    const glm::vec3 offset = direction * distance;

    const glm::vec3 targetUp = stabilizedOrbitUp(direction, orbitUp);
    const float targetAzimuth = wrapRadians(std::atan2(offset.x, offset.z));
    const float targetElevation = wrapRadians(std::asin(std::clamp(offset.y / distance, -1.0f, 1.0f)));
    const float targetZoom = keepOrthographic ? zoom : (moveSpeed / distance);

    orbitAnimationStartDirection = distance > 1e-6f ? glm::normalize(currentOffset) : direction;
    orbitAnimationTargetDirection = direction;
    orbitAnimationStartUp = orbitUp;
    orbitAnimationStartZoom = zoom;
    orbitAnimationTargetAzimuth = targetAzimuth;
    orbitAnimationTargetElevation = targetElevation;
    orbitAnimationTargetUp = targetUp;
    orbitAnimationTargetZoom = targetZoom;
    orbitAnimationT = 0.0f;
    orbitAnimationActive = true;
}

void Camera::snapToAxis(glm::vec3 axis) { snapToDirection(axis); }

RenderRay Camera::screenToRay(float screenX, float screenY) const {
    if (mode == Mode::Mode2D) {
        const glm::vec2 offset((screenX - screenSize.x * 0.5f) / zoom, (screenSize.y * 0.5f - screenY) / zoom);
        const glm::vec3 origin = orbitCenter + getPlanarRightAxis() * offset.x + getPlanarUpAxis() * offset.y;
        return RenderRay(origin, getForwardVector());
    }

    const float ndcX = (2.0f * screenX) / screenSize.x - 1.0f;
    const float ndcY = 1.0f - (2.0f * screenY) / screenSize.y;

    const glm::mat4 invProj = glm::inverse(getProjectionMatrix());

    const glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 rayEye = invProj * rayClip;

    rayEye.z = -1.0f;
    rayEye.w = 0.0f;

    const glm::mat4 invView = glm::inverse(getViewMatrix());
    const glm::vec3 rayDirWorld = glm::normalize(glm::vec3(invView * rayEye));

    return RenderRay(getEyePosition(), rayDirWorld);
}

glm::vec3 Camera::getPlanarRightAxis() const { return glm::normalize(glm::cross(getForwardVector(), orbitUp)); }

glm::vec3 Camera::getPlanarUpAxis() const { return glm::normalize(orbitUp); }

glm::vec3 Camera::getPlanarCenter() const { return orbitCenter; }
