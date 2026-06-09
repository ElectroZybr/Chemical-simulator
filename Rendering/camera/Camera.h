#pragma once

#include <glm/glm.hpp>
#include "Rendering/RenderRay.h"

class Renderer2D;
class Renderer3D;

class Camera {
    friend class Mouse;
    friend class Keyboard;

    friend Renderer2D;
    friend Renderer3D;

public:
    enum class Mode : uint8_t { Mode2D, Orbit, Free };

    Camera(float moveSpeed = 500.f, float zoomSpeed = 0.1f);

    void setSceneBounds(glm::vec3 size, glm::vec3 offset = glm::vec3{0.0f, 0.0f, 0.0f}) {
        sceneSize = size;
        sceneOffset = offset;
    }
    void resetView();

    void setScreenSize(glm::vec2 screenSize) { this->screenSize = screenSize; }
    glm::vec2 getScreenSize() const { return screenSize; }

    void move(glm::vec2 offset) { position += offset; }
    void move3D(glm::vec3 offset) { freePosition += offset; }

    void setPosition(glm::vec2 pos) { position = pos; };
    glm::vec2 getPosition() const { return position; };

    void setMode(Mode newMode);
    Mode getMode() const { return mode; }

    void orbitDrag(glm::ivec2 delta);
    void orbitRotate(float azimuthDelta, float elevationDelta);
    void freeDrag(glm::ivec2 delta); // для Free mode

    glm::vec3 screenToWorld(glm::ivec2 screenPos) const;
    glm::ivec2 worldToScreen(glm::vec3 worldPos) const;

    void zoomAt(float factor, glm::vec2 mousePos);
    float getZoom() const { return zoom; }
    void setZoom(float new_zoom);

    glm::vec3 getEyePosition() const;
    glm::vec3 getForwardVector() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

    RenderRay screenToRay(float screenX, float screenY) const;

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
