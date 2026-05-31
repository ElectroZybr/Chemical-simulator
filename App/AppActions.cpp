#include "AppActions.h"

#include "App/AppSignals.h"
#include "App/Scenes.h"
#include "App/capture/CaptureController.h"
#include "App/interaction/ToolsManager.h"
#include "App/save_system/AppStateIO.h"
#include "Engine/Simulation.h"
#include "GUI/interface/UiState.h"
#include "Rendering/2d/Renderer2DWGPU.h"
#include "Rendering/3d/Renderer3DWGPU.h"
#include "Rendering/WGPUContext.h"

namespace {
    void shiftAtoms(AtomStorage& atomStorage, Vec3f delta) {
        float* x = atomStorage.xData();
        float* y = atomStorage.yData();
        float* z = atomStorage.zData();
        const size_t atomCount = atomStorage.size();

        for (size_t i = 0; i < atomCount; ++i) {
            x[i] += delta.x;
            y[i] += delta.y;
            z[i] += delta.z;
        }
    }

    void applyResizeBox(Simulation& simulation, const Vec3f& newSize) {
        World& world = simulation.world();
        const Vec3f oldSize = world.getWorldSize();
        const Vec3f delta = (newSize - oldSize) * 0.5f;

        shiftAtoms(simulation.atoms(), delta);
        world.setRenderOffset(world.getRenderOffset() - delta);
        simulation.setSizeBox(newSize);
    }
}

namespace AppActions {
    void Handler::trackIOPanel(CaptureController& captureController, UiState& uiState, Simulation& simulation,
                               std::unique_ptr<IRenderer>& renderer) {
        track(AppSignals::UI::SaveSimulation.connect(
            [&](std::string_view path) { AppStateIO::save(captureController, uiState.scenePreviewRect, simulation, *renderer, path); }));
        track(AppSignals::UI::LoadSimulation.connect([&](std::string_view path) {
            AppStateIO::load(simulation, *renderer, path);
            ToolsManager::resetInteractionState();
        }));
        track(AppSignals::UI::ResizeBox.connect([&](const Vec3f& newSize) { applyResizeBox(simulation, newSize); }));
        track(AppSignals::UI::ClearSimulation.connect([&]() {
            simulation.clear();
            ToolsManager::resetInteractionState();
        }));
        track(AppSignals::UI::CreateGas.connect([&](int atomCount, AtomData::Type atomType, bool is3D, float density) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Scenes::randomGas(simulation, atomCount, atomType, is3D, 6.0f, 6.0f, density);
        }));
        track(AppSignals::UI::CreateCrystal.connect([&](int axisCount, AtomData::Type atomType, bool is3D) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Scenes::crystal(simulation, axisCount, atomType, is3D);
        }));
    }

    void Handler::trackToolsPanel(Simulation& simulation, std::unique_ptr<IRenderer>& renderer) {
        track(AppSignals::UI::SetRender.connect([&](RendererType type) {
            std::unique_ptr<IRenderer> newRenderer;
            switch (type) {
            case RendererType::Renderer2D:
                newRenderer = std::make_unique<Renderer2DWGPU>(simulation.world(), WGPUContext::instance().surfaceFormat());
                break;
            case RendererType::Renderer3D:
                newRenderer = std::make_unique<Renderer3DWGPU>(simulation.world(), WGPUContext::instance().surfaceFormat());
                break;
            }

            if (newRenderer) {
                ToolsManager::resetInteractionState();
                newRenderer->drawGrid = renderer->drawGrid;
                newRenderer->drawBonds = renderer->drawBonds;
                newRenderer->drawBox = renderer->drawBox;
                newRenderer->speedColorMode = renderer->speedColorMode;
                newRenderer->speedGradientMax = renderer->speedGradientMax;
                newRenderer->camera.setScreenSize(renderer->camera.getScreenSize());
                newRenderer->camera.resetView();
                renderer = std::move(newRenderer);
            }
        }));

        track(AppSignals::UI::SetCameraMode.connect([&](Camera::Mode mode) { renderer->camera.setMode(mode); }));
    }

    void Handler::trackSettingsPanel(GLFWwindow* window) {
        track(AppSignals::UI::ExitApplication.connect([window]() { glfwSetWindowShouldClose(window, GLFW_TRUE); }));
    }

    void Handler::trackKeyboard(Simulation& simulation) {
        track(AppSignals::Keyboard::StepPhysics.connect([&]() { simulation.update(); }));
    }

    void Handler::trackSimControlPanel(Simulation& simulation) {
        track(AppSignals::UI::StepPhysics.connect([&]() { simulation.update(); }));
    }

    Handler::Handler(GLFWwindow* window, CaptureController& captureController, Simulation& simulation, std::unique_ptr<IRenderer>& renderer,
                     UiState& uiState) {
        trackIOPanel(captureController, uiState, simulation, renderer);
        trackToolsPanel(simulation, renderer);
        trackSettingsPanel(window);
        trackSimControlPanel(simulation);
        trackKeyboard(simulation);
    }
}
