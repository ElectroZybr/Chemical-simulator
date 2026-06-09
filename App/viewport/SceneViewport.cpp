#include "App/viewport/SceneViewport.h"

#include "App/debug/DebugRuntime.h"
#include "App/interaction/ToolsManager.h"
#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/metrics/Profiler.h"
#include "GUI/interface/interface.h"
#include "Rendering/2d/Renderer2D.h"
#include "Rendering/3d/Renderer3D.h"
#include "Rendering/BaseRenderer.h"

SceneViewport::SceneViewport(RendererType type, CaptureController& captureController) : captureController_(&captureController), renderer_(createRenderer(type)) {}

void SceneViewport::setScreenSize(int width, int height) {
    renderer_->camera.setScreenSize(glm::vec2(static_cast<float>(width), static_cast<float>(height)));
}

void SceneViewport::resetView() { renderer_->camera.resetView(); }

void SceneViewport::syncScene(const Lattice::Simulation& simulation) { App::Viewport::syncRendererWithSimulation(*renderer_, simulation); }

void SceneViewport::renderFrame(const Lattice::Simulation& simulation, Interface& appInterface, const DebugViews& debugViews) {
    PROFILE_SCOPE("SceneViewport::renderFrame");

    UiState& uiState = appInterface.state();
    uiState.simStep = simulation.world().getSimStep();

    appInterface.update();

    if (ToolsManager::pickingSystem != nullptr) {
        App::Viewport::syncRendererWithSimulation(*renderer_, simulation, &ToolsManager::pickingSystem->getSelectedAtomIds());
    }
    else {
        App::Viewport::syncRendererWithSimulation(*renderer_, simulation);
    }

    refreshAtomDebugViews(debugViews, simulation);
    captureController_->renderFrame(*renderer_, [&]() {
        ToolsManager::overlay.draw();
        appInterface.draw(*renderer_);
    });
}

bool SceneViewport::setRendererType(RendererType type, const Lattice::Simulation& simulation) {
    std::unique_ptr<BaseRenderer> newRenderer = createRenderer(type);
    if (!newRenderer) {
        return false;
    }

    if (renderer_) {
        copyRenderSettings(*newRenderer, *renderer_);
        newRenderer->camera.setScreenSize(renderer_->camera.getScreenSize());
    }

    App::Viewport::syncRendererWithSimulation(*newRenderer, simulation);
    newRenderer->camera.resetView();
    renderer_ = std::move(newRenderer);
    return true;
}

std::unique_ptr<BaseRenderer> SceneViewport::createRenderer(RendererType type) {
    switch (type) {
    case RendererType::Renderer2D:
        return std::make_unique<Renderer2D>();
    case RendererType::Renderer3D:
        return std::make_unique<Renderer3D>();
    }

    return nullptr;
}

void SceneViewport::copyRenderSettings(BaseRenderer& destination, const BaseRenderer& source) {
    if (destination.getRenderDataCount() == 0 || source.getRenderDataCount() == 0) {
        return;
    }

    RenderData& target = destination.getRenderData(0);
    const RenderData& current = source.getRenderData(0);
    target.drawAtoms = current.drawAtoms;
    target.drawGrid = current.drawGrid;
    target.drawBonds = current.drawBonds;
    target.drawBox = current.drawBox;
    target.drawMemoryOrder = current.drawMemoryOrder;
    target.speedColorMode = current.speedColorMode;
    target.speedGradientMax = current.speedGradientMax;
}
