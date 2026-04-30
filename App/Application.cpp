#include "Application.h"

#include <cmath>
#include <cstdlib>

#include <imgui_impl_wgpu.h>

#include "App/AppActions.h"
#include "App/CreateWindow.h"
#include "App/Scenes.h"
#include "App/UserSettings.h"
#include "App/interaction/ToolsManager.h"
#include "Engine/Simulation.h"
#include "Engine/metrics/Profiler.h"
#include "GUI/interface/interface.h"
#include "GUI/io/keyboard/Keyboard.h"
#include "GUI/io/manager/EventManager.h"
#include "Rendering/2d/Renderer2DWGPU.h"
#include "Rendering/WGPUContext.h"
#include "capture/CaptureActions.h"
#include "capture/CaptureController.h"
#include "debug/CreateDebugPanels.h"
#include "debug/DebugRuntime.h"

using Clock = std::chrono::high_resolution_clock;

constexpr int FPS = 60;
constexpr int LPS = 20;

int Application::run() {
    GLFWwindow* window = createWindow();
    if (!window) {
        return EXIT_FAILURE;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    WGPUContext::instance().init(window, width, height);

    // инициализация систем
    SimBox box(Vec3f(50, 50, 6));
    Simulation simulation(box);
    CaptureController captureController;
    std::unique_ptr<IRenderer> renderer =
        std::make_unique<Renderer2DWGPU>(simulation.box(), *WGPUContext::instance().device(), WGPUContext::instance().surfaceFormat());
    Interface appInterface(window, simulation, renderer, captureController);
    AppActions::Handler appActions(window, captureController, simulation, renderer, appInterface.state());
    CaptureActions::Handler captureActions(captureController);
    if (appInterface.init() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    EventManager::init(window, renderer, appInterface);
    ToolsManager::init(window, simulation, renderer, appInterface);
    const DebugViews debugViews = createDebugViews(appInterface.debugPanel);

    // загрузка пользовательских настроек
    const UserSettings userSettings = UserSettingsIO::load();
    captureController.setSettings(userSettings.captureSettings);
    captureController.setOutputDirectory(userSettings.captureOutputDirectory);
    appInterface.setScenesDirectory(userSettings.scenesDirectory);
    renderer->drawGrid = userSettings.rendererDrawGrid;
    renderer->drawBonds = userSettings.rendererDrawBonds;
    renderer->speedColorMode = userSettings.rendererSpeedColorMode;
    renderer->speedGradientMax = userSettings.rendererSpeedGradientMax;
    simulation.setIntegrator(userSettings.simulationIntegrator);
    simulation.setBondFormationEnabled(userSettings.simulationBondFormationEnabled);
    simulation.setLJEnabled(userSettings.simulationLJEnabled);
    simulation.setCoulombEnabled(userSettings.simulationCoulombEnabled);
    appInterface.state().simulationSpeed = 100.0f;
    appInterface.state().pause = true;

    // создание сцены
    // Scenes::crystal(simulation, 400, AtomData::Type::Z, false);
    // Scenes::crystal(simulation, 400, AtomData::Type::Z, false, Scenes::CrystalPlane::XZ);
    Scenes::hexLattice(simulation, Vec3f(25, 25, 25), AtomData::Type::Z);
    // std::vector<Scenes::AtomTypeSpec> gasSpecs = {
    //     // {AtomData::Type::O, 0, 80.0f},    // 80% водорода
    //     {AtomData::Type::Na, 0, 50.0f},   // 10% натрия
    //     {AtomData::Type::Cl, 0, 50.0f}    // 10% хлора
    // };
    // Scenes::randomGasMixed(simulation, 500, gasSpecs, false, 6.0, 6.0, 1.0f, 5.0f, 0);
    // simulation.createAtom(Vec3f(24, 25, 3), Vec3f(1, 0, 0), AtomData::Type::Na);
    // simulation.createAtom(Vec3f(28, 25, 3), Vec3f(-1, 0, 0), AtomData::Type::Na);

    auto startTime = Clock::now();
    double renderAccum = 0.0;
    double physicsAccum = 0.0;
    double logAccum = 0.0;

    constexpr double renderInterval = 1.0 / FPS;
    constexpr double logInterval = 1.0 / LPS;

    renderer->camera.resetView();
    while (!glfwWindowShouldClose(window)) {
        Profiler::instance().beginFrame();

        auto currentTime = Clock::now();
        const float deltaTime = std::chrono::duration<float>(currentTime - startTime).count();
        startTime = currentTime;

        UiState& uiState = appInterface.state();
        physicsAccum += deltaTime;
        renderAccum += deltaTime;
        logAccum += deltaTime;

        EventManager::poll();
        EventManager::frame(deltaTime);
        captureController.update(deltaTime);
        captureController.syncUiState(uiState);
        captureController.handleToggleShortcut();

        // обновление физики
        const double physicsInterval = 1.0 / uiState.simulationSpeed;
        if (physicsAccum >= physicsInterval) {
            if (!uiState.pause) {
                simulation.update();
            }
            physicsAccum = 0.0;
        }

        // отрисовка кадра
        if (renderAccum >= renderInterval) {
            PROFILE_SCOPE("Application::RenderFrame");
            renderAccum -= renderInterval;

            uiState.simStep = simulation.getSimStep();
            appInterface.update();
            refreshAtomDebugViews(debugViews, simulation);

            WGPUContext& ctx = WGPUContext::instance();

            // получаем surface текстуру один раз на кадр
            wgpu::SurfaceTexture surfaceTex;
            ctx.surface()->getCurrentTexture(&surfaceTex);
            wgpu::Texture surfaceTexture(surfaceTex.texture);

            // - нет захвата → возвращает view от surface напрямую
            // - идёт захват → возвращает view intermediate текстуры
            wgpu::TextureView renderTarget = captureController.acquireRenderTarget(surfaceTexture);

            renderer->drawShot(renderTarget, *ctx.depthView(), simulation.atoms(), simulation.bonds(), simulation.box());
            ToolsManager::pickingSystem->getOverlay().draw();
            ImGui::Render();
            auto* wgpuRenderer = static_cast<RendererWGPU*>(renderer.get());
            ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), wgpuRenderer->getCurrentPass());
            renderer->endFrame();

            // захват кадра для видео
            captureController.onFrameRendered(surfaceTexture);

            ctx.present();
            ctx.processEvents();
        }

        Profiler::instance().endFrame();

        // обновление логов и данных счетчиков
        if (logAccum >= logInterval) {
            logAccum -= logInterval;
            refreshSimulationDebugViews(debugViews, simulation);
        }
    }

    captureController.stop();
    UserSettingsIO::save(UserSettings{
        .captureOutputDirectory = captureController.outputDirectory(),
        .scenesDirectory = appInterface.scenesDirectory(),
        .captureSettings = captureController.settings(),
        .rendererDrawGrid = renderer->drawGrid,
        .rendererDrawBonds = renderer->drawBonds,
        .rendererSpeedColorMode = renderer->speedColorMode,
        .rendererSpeedGradientMax = renderer->speedGradientMax,
        .simulationIntegrator = simulation.getIntegrator(),
        .simulationBondFormationEnabled = simulation.isBondFormationEnabled(),
        .simulationLJEnabled = simulation.isLJEnabled(),
        .simulationCoulombEnabled = simulation.isCoulombEnabled(),
    });
    appInterface.shutdown();
    return 0;
}
