#include "Mouse.h"

#include <cmath>
#include <iostream>

#include "App/interaction/ToolsManager.h"
#include "GUI/interface/interface.h"

GLFWwindow* Mouse::window = nullptr;
std::unique_ptr<BaseRenderer>* Mouse::renderer = nullptr;
Interface* Mouse::appInterface = nullptr;
GLFWmousebuttonfun Mouse::imgui_mouse_callback = nullptr;
GLFWcursorposfun Mouse::imgui_cursor_pos_callback = nullptr;
GLFWscrollfun Mouse::imgui_scroll_callback = nullptr;

void Mouse::init(GLFWwindow* w, std::unique_ptr<BaseRenderer>& r, Interface& appInterface) {
    window = w;
    renderer = &r;
    Mouse::appInterface = &appInterface;

    imgui_mouse_callback = glfwSetMouseButtonCallback(window, onMouseButton);
    imgui_cursor_pos_callback = glfwSetCursorPosCallback(window, onMouseMove);
    imgui_scroll_callback = glfwSetScrollCallback(window, onScroll);
}

void Mouse::onMouseButton(GLFWwindow*, int button, int action, int mods) {
    if (imgui_mouse_callback) {
        imgui_mouse_callback(window, button, action, mods);
    }
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    const Vec2i mouse_pos = Mouse::getMousePos();
    std::unique_ptr<BaseRenderer>& rend = *renderer;

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            ToolsManager::onLeftPressed(mouse_pos);
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT && appInterface != nullptr && !appInterface->state().cursorHovered &&
            !ToolsManager::blocksCameraControls()) {
            if (!ToolsManager::onRightPressed(mouse_pos)) {
                rend->camera.isDragging = true;
                rend->camera.dragStartPixelPos = {mouse_pos.x, mouse_pos.y};
                rend->camera.dragStartCameraPos = rend->camera.position;
            }
        }
    }

    if (action == GLFW_RELEASE) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            ToolsManager::onLeftReleased(mouse_pos);
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            rend->camera.isDragging = false;
        }
    }
}

void Mouse::onMouseMove(GLFWwindow*, double xpos, double ypos) {
    if (imgui_cursor_pos_callback) {
        imgui_cursor_pos_callback(window, xpos, ypos);
    }

    std::unique_ptr<BaseRenderer>& rend = *renderer;
    if (!rend->camera.isDragging) {
        return;
    }

    const Vec2i currentPixelPos(xpos, ypos);
    Vec2i deltaPixel(currentPixelPos - Vec2i(rend->camera.dragStartPixelPos));

    if (rend->camera.mode == Camera::Mode::Orbit) {
        rend->camera.orbitDrag(deltaPixel);
        rend->camera.dragStartPixelPos = {currentPixelPos.x, currentPixelPos.y};
    }
    else if (rend->camera.mode == Camera::Mode::Free) {
        rend->camera.freeDrag(deltaPixel);
        rend->camera.dragStartPixelPos = {currentPixelPos.x, currentPixelPos.y};
    }
    else {
        Vec3f deltaWorld = ToolsManager::screenToWorld(Vec2i(rend->camera.dragStartPixelPos)) - ToolsManager::screenToWorld(currentPixelPos);
        rend->camera.position = rend->camera.dragStartCameraPos + glm::vec2(deltaWorld.x, deltaWorld.y);
    }
}

void Mouse::onScroll(GLFWwindow*, double xoffset, double yoffset) {
    if (imgui_scroll_callback) {
        imgui_scroll_callback(window, xoffset, yoffset);
    }

    std::unique_ptr<BaseRenderer>& rend = *renderer;
    constexpr float kFreeWheelMoveScale = 0.008f;

    if (appInterface != nullptr && !appInterface->state().cursorHovered && !ToolsManager::blocksCameraControls()) {
        if (rend->camera.getMode() == Camera::Mode::Free) {
            const float wheelStep = rend->camera.moveSpeed * kFreeWheelMoveScale;
            const float distance = static_cast<float>(yoffset) * wheelStep;

            const Vec3f forward(rend->camera.getForwardVector());
            rend->camera.move3D(forward * distance);
        }
        else {
            const Vec2i mouse_pos = getMousePos();
            rend->camera.zoomAt(static_cast<float>(yoffset), Vec2f(mouse_pos));
        }
    }
}

void Mouse::onFrame(float deltaTime) {
    const Vec2i mouse_pos = getMousePos();
    ToolsManager::onFrame(mouse_pos, deltaTime);
}

void Mouse::logMousePos() {
    const Vec2i mouse_pos = getMousePos();
    Vec3f world_pos = ToolsManager::screenToWorld(mouse_pos);
    std::cout << "<Mouse pos>"
              << " Screen: "
              << "X " << mouse_pos.x << "Y " << mouse_pos.y << " | World: "
              << "X " << world_pos.x << "Y " << world_pos.y << std::endl;
}
