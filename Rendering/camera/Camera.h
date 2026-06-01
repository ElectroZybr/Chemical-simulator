#pragma once

#include <glm/glm.hpp>

#include "Lattice/Engine/math/Ray.h"
#include "Lattice/Engine/math/Vec2.h"
#include "Lattice/Engine/math/Vec3.h"
#include "Rendering/RenderMath.h"

class Renderer2D;
class Renderer3DWGPU;
class Renderer2DWGPU;

class Camera {
    friend class Mouse;
    friend class Keyboard;

    friend Renderer2D;
    friend Renderer3DWGPU;

    friend Renderer2DWGPU;

public:
    enum class Mode : uint8_t { Mode2D, Orbit, Free };

    Camera(float moveSpeed = 500.f, float zoomSpeed = 0.1f);

    void setSceneBounds(glm::vec3 size, glm::vec3 offset = glm::vec3{0.0f, 0.0f, 0.0f}) {
        sceneSize = size;
        sceneOffset = offset;
    }
    void setSceneBounds(Vec3f size, Vec3f offset = Vec3f{}) {
        setSceneBounds(glm::vec3(size.x, size.y, size.z), glm::vec3(offset.x, offset.y, offset.z));
    }
    void resetView();

    void setScreenSize(glm::vec2 screenSize) { this->screenSize = screenSize; }
    void setScreenSize(Vec2f newScreenSize) { setScreenSize(glm::vec2(newScreenSize.x, newScreenSize.y)); }
    glm::vec2 getScreenSize() const { return screenSize; }

    void move(glm::vec2 offset) { position += offset; }
    void move(Vec2f offset) { move(glm::vec2(offset.x, offset.y)); }
    void move3D(glm::vec3 offset) { freePosition += offset; }
    void move3D(Vec3f offset) { move3D(glm::vec3(offset.x, offset.y, offset.z)); }

    void setPosition(glm::vec2 pos) { position = pos; };
    void setPosition(Vec2f pos) { setPosition(glm::vec2(pos.x, pos.y)); }
    glm::vec2 getPosition() const { return position; };

    void setMode(Mode newMode);
    Mode getMode() const { return mode; }

    void orbitDrag(glm::ivec2 delta);
    void orbitDrag(Vec2i delta) { orbitDrag(glm::ivec2(delta.x, delta.y)); }
    void orbitRotate(float azimuthDelta, float elevationDelta);
    void freeDrag(glm::ivec2 delta); // для Free mode
    void freeDrag(Vec2i delta) { freeDrag(glm::ivec2(delta.x, delta.y)); }

    glm::vec3 screenToWorld(glm::ivec2 screenPos) const;
    Vec3f screenToWorld(Vec2i screenPos) const { return Vec3f(screenToWorld(glm::ivec2(screenPos.x, screenPos.y))); }
    glm::ivec2 worldToScreen(glm::vec3 worldPos) const;
    Vec2i worldToScreen(Vec3f worldPos) const { return Vec2i(worldToScreen(glm::vec3(worldPos.x, worldPos.y, worldPos.z))); }

    void zoomAt(float factor, glm::vec2 mousePos);
    void zoomAt(float factor, Vec2f mousePos) { zoomAt(factor, glm::vec2(mousePos.x, mousePos.y)); }
    float getZoom() const { return zoom; }
    void setZoom(float new_zoom);

    glm::vec3 getEyePosition() const;
    glm::vec3 getForwardVector() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

    RenderRay screenToRay(float screenX, float screenY) const;
    Ray screenToRay(Vec2i screenPos) const {
        const RenderRay ray = screenToRay(static_cast<float>(screenPos.x), static_cast<float>(screenPos.y));
        return Ray(Vec3f(ray.origin), Vec3f(ray.dir));
    }

private:
    glm::vec2 screenSize;
    glm::vec2 position;
    glm::vec3 freePosition{0.f, 0.f, -100.f};
    glm::vec3 orbitCenter{0.f, 0.f, 0.f};
    glm::vec3 orbitUp{0.f, 1.f, 0.f};
    float zoom;
    float speed;
    float moveSpeed;
    float zoomSpeed;

    bool isDragging;
    glm::vec2 lastMousePos;

    glm::ivec2 dragStartPixelPos;
    glm::vec2 dragStartCameraPos;

    Mode mode = Mode::Mode2D;
    glm::vec3 sceneSize{1.0f, 1.0f, 1.0f};
    glm::vec3 sceneOffset{0.0f, 0.0f, 0.0f};

    // Orbit / Free
    float azimuth = 0.f;
    float elevation = 0.f;

    // Перспектива
    static constexpr float FOV_ORBIT = 45.f;
    static constexpr float FOV_FREE = 60.f;
    static constexpr float NEAR = 0.1f;
    static constexpr float FAR = 10000.f;
};
