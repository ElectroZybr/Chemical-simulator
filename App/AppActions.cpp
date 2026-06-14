#include "AppActions.h"

#include <cmath>

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
    glm::vec3 clampBoxSize(glm::vec3 size) {
        return glm::max(size, glm::vec3(1.0f));
    }

    glm::vec3 moveTowards(glm::vec3 current, glm::vec3 target, float maxStep) {
        const glm::vec3 delta = target - current;
        return glm::vec3(
            std::abs(delta.x) <= maxStep ? target.x : current.x + std::copysign(maxStep, delta.x),
            std::abs(delta.y) <= maxStep ? target.y : current.y + std::copysign(maxStep, delta.y),
            std::abs(delta.z) <= maxStep ? target.z : current.z + std::copysign(maxStep, delta.z)
        );
    }

    void shiftAtoms(AtomStorage& atomStorage, glm::vec3 delta) {
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

    void applyResizeBox(Lattice::Simulation& simulation, const glm::vec3& newSize) {
        World& world = simulation.world();
        const glm::vec3 clampedSize = clampBoxSize(newSize);
        const glm::vec3 oldSize = world.getWorldSize();
        const glm::vec3 delta = (clampedSize - oldSize) * 0.5f;

        shiftAtoms(simulation.atoms(), delta);
        world.setRenderOffset(world.getRenderOffset() - delta);
        simulation.setSizeBox(clampedSize);
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
    void Handler::applySmoothResizeStep(Lattice::Simulation& simulation) {
        if (!smoothResizeActive_) {
            return;
        }

        const float maxStep = smoothResizeMaxSpeed_ * std::max(simulation.getDt(), 0.0f);
        if (maxStep <= 0.0f) {
            return;
        }

        const glm::vec3 nextSize = moveTowards(simulation.world().getWorldSize(), smoothResizeTarget_, maxStep);
        applyResizeBox(simulation, nextSize);
        if (nextSize == smoothResizeTarget_) {
            smoothResizeActive_ = false;
        }
    }

    void Handler::trackIOPanel(CaptureController& captureController, UiState& uiState, Lattice::Simulation& simulation, SceneViewport& renderer) {
        track(AppSignals::UI::SaveSimulation.connect(
            [&](std::string_view path) { AppStateIO::save(captureController, uiState.scenePreviewRect, simulation, renderer.renderer(), path); }));
        track(AppSignals::UI::LoadSimulation.connect([&](std::string_view path) {
            AppStateIO::load(simulation, renderer.renderer(), path);
            renderer.syncScene(simulation);
            ToolsManager::resetInteractionState();
        }));
        track(AppSignals::UI::ResizeBox.connect([&](const glm::vec3& newSize) {
            smoothResizeActive_ = false;
            applyResizeBox(simulation, newSize);
        }));
        track(AppSignals::UI::SmoothResizeBox.connect([&](const glm::vec3& newSize, float maxSpeed) {
            smoothResizeTarget_ = clampBoxSize(newSize);
            smoothResizeMaxSpeed_ = std::max(maxSpeed, 0.0f);
            smoothResizeActive_ = true;
            if (smoothResizeMaxSpeed_ <= 0.0f) {
                applyResizeBox(simulation, smoothResizeTarget_);
                smoothResizeActive_ = false;
            }
        }));
        track(AppSignals::UI::ClearSimulation.connect([&]() {
            simulation.clear();
            ToolsManager::resetInteractionState();
            smoothResizeActive_ = false;
        }));
        track(AppSignals::UI::CreateGas.connect([&](int atomCount, AtomData::Type atomType, bool is3D, float density) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Generators::randomGas(simulation, atomCount, atomType, is3D, 6.0f, 6.0f, density);
        }));
        track(AppSignals::UI::CreateMixedGas.connect([&](int atomCount, std::vector<Generators::AtomTypeSpec> atomSpecs, bool is3D, float density) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Generators::randomGasMixed(simulation, atomCount, atomSpecs, is3D, 6.0f, 6.0f, density);
        }));
        track(AppSignals::UI::CreateMassive.connect([&](glm::ivec3 axisCounts, AtomData::Type atomType, bool is3D) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Generators::massive(simulation, axisCounts, atomType, is3D);
        }));
        track(AppSignals::UI::CreateHexLattice.connect([&](glm::ivec3 axisCounts, AtomData::Type atomType) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Generators::hexLattice(simulation, axisCounts, atomType);
        }));
        track(AppSignals::UI::CreateTriangularBipyramidCrystal.connect([&](int axisCount, AtomData::Type atomType) {
            simulation.clear();
            ToolsManager::resetInteractionState();
            Generators::triangularBipyramidCrystal(simulation, axisCount, atomType);
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

    void Handler::trackSettingsPanel(bool& exitRequested) {
        track(AppSignals::UI::ExitApplication.connect([&exitRequested]() { exitRequested = true; }));
    }

    void Handler::trackKeyboard(Lattice::Simulation& simulation) {
        track(AppSignals::Keyboard::StepPhysics.connect([&]() {
            applySmoothResizeStep(simulation);
            simulation.update();
        }));
    }

    void Handler::trackSimControlPanel(Lattice::Simulation& simulation) {
        track(AppSignals::UI::StepPhysics.connect([&]() {
            applySmoothResizeStep(simulation);
            simulation.update();
        }));
    }

    Handler::Handler(CaptureController& captureController, Lattice::Simulation& simulation, SceneViewport& renderer, UiState& uiState,
                     bool& exitRequested) {
        trackIOPanel(captureController, uiState, simulation, renderer);
        trackToolsPanel(simulation, renderer);
        trackSettingsPanel(exitRequested);
        trackSimControlPanel(simulation);
        trackKeyboard(simulation);
    }

    void Handler::updateSimulationStep(Lattice::Simulation& simulation) {
        applySmoothResizeStep(simulation);
    }
}
