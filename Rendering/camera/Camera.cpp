#include "Camera.h"

#include <algorithm>
#include <cmath>

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
}

Camera::Camera(float moveSpeed, float zoomSpeed) : moveSpeed(moveSpeed), zoomSpeed(zoomSpeed), isDragging(false), lastMousePos(0, 0) {}

void Camera::resetView() {
    azimuth = 0.f;
    elevation = 0.f;
    orbitUp = glm::vec3(0.f, 1.f, 0.f);

    const float max_side = std::max({sceneSize.x, sceneSize.y, sceneSize.z});
    const float distance = (max_side * 0.5f * 1.1f) / std::tan(glm::radians(Camera::FOV_ORBIT) * 0.5f);
    orbitCenter = sceneOffset + sceneSize * 0.5f;
    position = glm::vec2(orbitCenter);

    if (mode == Camera::Mode::Mode2D) {
        constexpr float margin = 0.85f;

        const float zoomX = (screenSize.x * margin) / sceneSize.x;
        const float zoomY = (screenSize.y * margin) / sceneSize.y;

        setZoom(std::min(zoomX, zoomY));
    }
    else {
        const float safeDistance = std::max(distance, 1e-3f);
        setZoom(moveSpeed / safeDistance);
        const glm::vec3 orbitOffset(std::cos(elevation) * std::sin(azimuth), std::sin(elevation), std::cos(elevation) * std::cos(azimuth));
        freePosition = orbitCenter + orbitOffset * safeDistance;
    }
}

void Camera::setZoom(float new_zoom) {
    zoom = std::clamp(new_zoom, 0.1f, 10000.f);
    speed = moveSpeed / zoom;
}

void Camera::setMode(Mode newMode) {
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
    // Изменяем уровень зума с учетом направления к курсору
    zoom *= (1.f + factor * zoomSpeed);
    zoom = std::clamp(zoom, 1.f, 500.f);
    speed = moveSpeed / zoom;

    // Плавное следование за указателем мыши при зуме
    if (zoom > 1.f && zoom < 500.f) {
        glm::vec2 deltaPos(mousePos - screenSize * 0.5f);
        deltaPos.y *= -1.f;
        position += deltaPos * 0.1f / zoom * factor;
    }
}

void Camera::orbitDrag(glm::ivec2 delta) {
    constexpr float sensitivity = 0.005f;
    orbitRotate(-delta.x * sensitivity, -delta.y * sensitivity);
}

void Camera::orbitRotate(float azimuthDelta, float elevationDelta) {
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
    constexpr float sensitivity = 0.003f;
    azimuth -= delta.x * sensitivity;
    elevation += delta.y * sensitivity;
    elevation = std::clamp(elevation, -1.5f, 1.5f);
}

// Для 3д режимов возвращает cameraPos + cameraDir * 10
glm::vec3 Camera::screenToWorld(glm::ivec2 screenPos) const {
    if (mode == Mode::Mode2D) {
        glm::vec2 offset = glm::vec2(screenPos) - screenSize * 0.5f;
        offset.y *= -1.f;
        const glm::vec2 w = position + offset / zoom;
        return glm::vec3(w, 0.0f);
    }

    const RenderRay ray = screenToRay(screenPos.x, screenPos.y);
    return ray.at(100.0);
}

glm::ivec2 Camera::worldToScreen(glm::vec3 worldPos) const {
    if (mode == Mode::Mode2D) {
        glm::vec2 offset = glm::vec2(worldPos) - position;
        offset.y *= -1.f;
        const glm::vec2 s = offset * zoom + screenSize * 0.5f;
        return glm::ivec2(s);
    }

    const glm::vec4 clip = getProjectionMatrix() * getViewMatrix() * glm::vec4(worldPos.x, worldPos.y, worldPos.z, 1.f);

    if (clip.w <= 0.f) {
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
    const float r = moveSpeed / zoom;
    return glmCenter + r * glm::vec3(std::cos(elevation) * std::sin(azimuth), std::sin(elevation), std::cos(elevation) * std::cos(azimuth));
}

glm::vec3 Camera::getForwardVector() const {
    return glm::normalize(glm::vec3(-std::cos(elevation) * std::sin(azimuth), -std::sin(elevation),
                                   -std::cos(elevation) * std::cos(azimuth)));
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
    const float fov = (mode == Mode::Free) ? FOV_FREE : FOV_ORBIT;
    return glm::perspective(glm::radians(fov), screenSize.x / screenSize.y, NEAR, FAR);
}

RenderRay Camera::screenToRay(float screenX, float screenY) const {
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
