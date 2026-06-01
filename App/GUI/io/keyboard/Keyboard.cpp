#include "Keyboard.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "App/AppSignals.h"
#include "GUI/interface/interface.h"

std::unique_ptr<BaseRenderer>* Keyboard::render = nullptr;
Interface* Keyboard::appInterface = nullptr;
GLFWwindow* Keyboard::window = nullptr;
GLFWkeyfun Keyboard::imgui_key_callback = nullptr;

void Keyboard::init(GLFWwindow* window, std::unique_ptr<BaseRenderer>& r, Interface& appInterface) {
    render = &r;
    Keyboard::appInterface = &appInterface;
    Keyboard::window = window;

    imgui_key_callback = glfwSetKeyCallback(window, Keyboard::onKey);
}

bool Keyboard::isPressed(int key) {
    if (!window) {
        return false;
    }
    return glfwGetKey(window, key) == GLFW_PRESS;
}

void Keyboard::onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (imgui_key_callback) {
        imgui_key_callback(window, key, scancode, action, mods);
    }

    if (action != GLFW_PRESS) {
        return;
    }
    if (appInterface == nullptr) {
        return;
    }
    if (ImGui::GetIO().WantTextInput) {
        return;
    }

    UiState& uiState = appInterface->state();
    const bool ctrlHeld = (mods & GLFW_MOD_CONTROL) != 0;

    if (ctrlHeld && key == GLFW_KEY_S) {
        appInterface->fileDialog.openSave();
        return;
    }
    if (ctrlHeld && key == GLFW_KEY_O) {
        appInterface->fileDialog.openLoad();
        return;
    }

    if (key == GLFW_KEY_P) {
        appInterface->debugPanel.toggle();
    }
    else if (key == GLFW_KEY_ESCAPE) {
        AppSignals::UI::ExitApplication.emit();
    }
    else if (key == GLFW_KEY_SPACE) {
        uiState.pause = !uiState.pause;
    }
    else if (key == GLFW_KEY_RIGHT && uiState.pause) {
        AppSignals::Keyboard::StepPhysics.emit();
    }
}

void Keyboard::onFrame(float deltaTime) {
    std::unique_ptr<BaseRenderer>& rend = *render;
    constexpr float kFreeMoveSpeedScale = 0.8f;

    if (rend->camera.mode == Camera::Mode::Orbit) {
        float rotSpeed = 1.5f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            rend->camera.orbitRotate(0.0f, rotSpeed);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            rend->camera.orbitRotate(0.0f, -rotSpeed);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            rend->camera.orbitRotate(-rotSpeed, 0.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            rend->camera.orbitRotate(rotSpeed, 0.0f);
        }
    }
    else if (rend->camera.mode == Camera::Mode::Free) {
        const glm::vec3 forward = rend->camera.getForwardVector();
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
        const float s = rend->camera.speed * deltaTime * kFreeMoveSpeedScale;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            rend->camera.move3D(glm::vec3(forward.x, forward.y, forward.z) * s);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            rend->camera.move3D(glm::vec3(-forward.x, -forward.y, -forward.z) * s);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            rend->camera.move3D(glm::vec3(-right.x, -right.y, -right.z) * s);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            rend->camera.move3D(glm::vec3(right.x, right.y, right.z) * s);
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            rend->camera.move3D(glm::vec3(0.f, -s, 0.f));
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            rend->camera.move3D(glm::vec3(0.f, s, 0.f));
        }
    }
    else if (rend->camera.mode == Camera::Mode::Mode2D) {
        float deltaSpeed = rend->camera.speed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            rend->camera.move(Vec2f(0.0f, deltaSpeed));
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            rend->camera.move(Vec2f(0.0f, -deltaSpeed));
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            rend->camera.move(Vec2f(-deltaSpeed, 0.0f));
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            rend->camera.move(Vec2f(deltaSpeed, 0.0f));
        }
    }
}
