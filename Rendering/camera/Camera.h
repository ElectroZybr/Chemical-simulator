#pragma once

#include <glm/glm.hpp>

#include "Engine/math/Ray.h"

class World;
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

    Camera(World& simBox, float moveSpeed = 500.f, float zoomSpeed = 0.1f);

    void resetView();

    void setScreenSize(Vec2f screenSize) { this->screenSize = screenSize; }
    Vec2f getScreenSize() const { return screenSize; }

    void move(Vec2f offset) { position += offset; }
    void move3D(Vec3f offset) { freePosition += offset; }

    void setPosition(Vec2f pos) { position = pos; };
    Vec2f getPosition() const { return position; };

    void setMode(Mode newMode) { mode = newMode; }
    Mode getMode() const { return mode; }

    void orbitDrag(Vec2i delta);
    void freeDrag(Vec2i delta); // для Free mode

    Vec3f screenToWorld(Vec2i screenPos) const;
    Vec2i worldToScreen(Vec3f worldPos) const;

    void zoomAt(float factor, Vec2f mousePos);
    float getZoom() const { return zoom; }
    void setZoom(float new_zoom);

    glm::vec3 getEyePosition() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

    Ray screenToRay(float screenX, float screenY) const;

private:
    Vec2f screenSize;
    Vec2f position;
    Vec3f freePosition{0.f, 0.f, -100.f};
    float zoom;
    float speed;
    float moveSpeed;
    float zoomSpeed;

    bool isDragging;
    Vec2f lastMousePos;

    Vec2i dragStartPixelPos;
    Vec2f dragStartCameraPos;

    Mode mode = Mode::Mode2D;
    World& world;

    // Orbit / Free
    float azimuth = 0.f;
    float elevation = 0.f;

    // Перспектива
    static constexpr float FOV_ORBIT = 45.f;
    static constexpr float FOV_FREE = 60.f;
    static constexpr float NEAR = 0.1f;
    static constexpr float FAR = 10000.f;
};
