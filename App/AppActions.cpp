#include "AppActions.h"

#include "App/AppSignals.h"
#include "Lattice/Generators/Generators.h"
#include "App/capture/CaptureOutputPath.h"
#include "App/capture/CaptureController.h"
#include "App/interaction/ToolsManager.h"
#include "App/viewport/SceneViewport.h"
#include "App/save_system/AppStateIO.h"
#include "Lattice/Engine/Simulation.h"
#include "GUI/interface/UiState.h"

namespace {

    void applyNewBoxSize(Lattice::Simulation& simulation, const glm::vec3& newSize) {
        simulation.setBoxSize(newSize);
    }

    void applyNewTargetBoxSize(Lattice::Simulation& simulation, const glm::vec3& newSize) {
        simulation.setTargetBoxSize(newSize);
    }

    void toggleXYZRecording(CaptureController& captureController, Lattice::Simulation& simulation) {
        if (simulation.isXYZRecording()) {
            simulation.stopXYZRecording();
            return;
        }

        std::error_code fsError;
        const std::filesystem::path outputDirectory = captureController.outputDirectory();
        std::filesystem::create_directories(outputDirectory, fsError);
        if (fsError) {
            return;
        }

        simulation.startXYZRecording(capture_utils::makeDatedCaptureOutputPath(outputDirectory, ".xyz").string());
    }
}

namespace AppActions {
    void Handler::trackIOPanel(CaptureController& captureController, UiState& uiState, Lattice::Simulation& simulation, SceneViewport& renderer) {
        track(AppSignals::UI::SaveSimulation.connect(
            [&](std::string_view path) { AppStateIO::save(captureController, uiState.scenePreviewRect, simulation, renderer.renderer(), path); }));
        track(AppSignals::UI::LoadSimulation.connect([&](std::string_view path) {
            AppStateIO::load(simulation, renderer.renderer(), path);
            renderer.syncScene(simulation);
            ToolsManager::resetInteractionState();
        }));
        track(AppSignals::UI::NewBoxSize.connect([&](const glm::vec3& newSize) { applyNewBoxSize(simulation, newSize); }));
        track(AppSignals::UI::NewTargetBoxSize.connect([&](const glm::vec3& newSize) { applyNewTargetBoxSize(simulation, newSize); }));
        track(AppSignals::UI::ClearSimulation.connect([&]() {
            simulation.clear();
            ToolsManager::resetInteractionState();
        }));
        track(AppSignals::UI::CreateGas.connect([&](int atomCount, AtomData::Type atomType, bool is3D, float density) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Generators::randomGas(simulation, atomCount, atomType, is3D, 6.0f, 6.0f, density);
        }));
        track(AppSignals::UI::CreateCrystal.connect([&](int axisCount, AtomData::Type atomType, bool is3D) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Generators::massive(simulation, axisCount, atomType, is3D);
        }));
        track(AppSignals::Capture::ToggleXYZRecording.connect([&]() { toggleXYZRecording(captureController, simulation); }));
    }

    void Handler::trackToolsPanel(Lattice::Simulation& simulation, SceneViewport& renderer) {
        track(AppSignals::UI::SetRender.connect([&](RendererType type) {
            const SceneViewport::RendererType rendererType =
                (type == RendererType::Renderer2D) ? SceneViewport::RendererType::Renderer2D : SceneViewport::RendererType::Renderer3D;
            if (renderer.setRendererType(rendererType, simulation)) {
                ToolsManager::resetInteractionState();
            }
        }));

        track(AppSignals::UI::SetCameraMode.connect([&](Camera::Mode mode) { renderer.renderer().camera.setMode(mode); }));
    }

    void Handler::trackSettingsPanel(GLFWwindow* window) {
        track(AppSignals::UI::ExitApplication.connect([window]() { glfwSetWindowShouldClose(window, GLFW_TRUE); }));
    }

    void Handler::trackKeyboard(Lattice::Simulation& simulation) {
        track(AppSignals::Keyboard::StepPhysics.connect([&]() { simulation.update(); }));
    }

    void Handler::trackSimControlPanel(Lattice::Simulation& simulation) {
        track(AppSignals::UI::StepPhysics.connect([&]() { simulation.update(); }));
    }

    Handler::Handler(GLFWwindow* window, CaptureController& captureController, Lattice::Simulation& simulation, SceneViewport& renderer, UiState& uiState) {
        trackIOPanel(captureController, uiState, simulation, renderer);
        trackToolsPanel(simulation, renderer);
        trackSettingsPanel(window);
        trackSimControlPanel(simulation);
        trackKeyboard(simulation);
    }
}
