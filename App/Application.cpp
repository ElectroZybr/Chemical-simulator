#include "Application.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "App/AppActions.h"
#include "App/CreateWindow.h"
#include "App/WindowController.h"
#include "Lattice/Generators/Generators.h"
#include "App/UserSettings.h"
#include "App/viewport/SceneViewport.h"
#include "App/interaction/ToolsManager.h"
#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/metrics/Profiler.h"
#include "GUI/interface/interface.h"
#include "GUI/io/keyboard/Keyboard.h"
#include "GUI/io/manager/EventManager.h"
#include "Rendering/backend/WGPUContext.h"
#include "capture/CaptureActions.h"
#include "capture/CaptureController.h"
#include "debug/CreateDebugPanels.h"
#include "debug/DebugRuntime.h"

using Clock = std::chrono::high_resolution_clock;

constexpr int FPS = 60;
constexpr int LPS = 20;

namespace {
    uint32_t makeXYZStepInterval(float simulationStepsPerSecond, int captureFps) {
        const float sanitizedStepsPerSecond = std::max(simulationStepsPerSecond, 1.0f);
        const int sanitizedCaptureFps = std::max(captureFps, 1);
        return std::max(1u, static_cast<uint32_t>(std::lround(sanitizedStepsPerSecond / static_cast<float>(sanitizedCaptureFps))));
    }

    void presentInitializedWindow(GLFWwindow* window, SceneViewport& renderer, Lattice::Simulation& simulation, Interface& appInterface,
                                  const DebugViews& debugViews) {
        renderer.renderFrame(simulation, appInterface, debugViews);
        glfwShowWindow(window);
        glfwFocusWindow(window);
    }
}

int Application::run() {
    const UserSettings userSettings = UserSettingsIO::load();
    GLFWwindow* window = createWindow(userSettings.windowState);
    if (!window) {
        return EXIT_FAILURE;
    }
    WindowController::init(window, userSettings.windowState);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    WGPUContext::instance().init(window, width, height);

    // инициализация систем
    Lattice::Simulation simulation;

    simulation.createWorld(glm::vec3(120.0f, 120.0f, 120.0f));

    CaptureController captureController;
    const SceneViewport::RendererType initialRendererType =
        userSettings.rendererUse3D ? SceneViewport::RendererType::Renderer3D : SceneViewport::RendererType::Renderer2D;
    SceneViewport renderer(initialRendererType, captureController);
    renderer.syncScene(simulation);

    Interface appInterface(window, simulation, renderer.rendererHandle(), captureController);
    appInterface.toolsPanel.setRendererType(renderer.renderer().camera.getMode() == Camera::Mode::Mode2D ? RendererType::Renderer2D : RendererType::Renderer3D);

    bool exitRequested = false;
    AppActions::Handler appActions(captureController, simulation, renderer, appInterface.state(), exitRequested);
    CaptureActions::Handler captureActions(captureController);
    if (appInterface.init() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    appInterface.setUiScaleMultiplier(userSettings.interfaceScale);
    EventManager::init(window, renderer.rendererHandle(), appInterface);
    ToolsManager::init(window, simulation, renderer.rendererHandle(), appInterface);
    const DebugViews debugViews = createDebugViews(appInterface.debugPanel);

    // загрузка пользовательских настроек
    captureController.setSettings(userSettings.captureSettings);
    captureController.setOutputDirectory(userSettings.captureOutputDirectory);
    appInterface.setScenesDirectory(userSettings.scenesDirectory);
    renderer.renderer().getRenderData(0).drawAtoms = userSettings.rendererDrawAtoms;
    renderer.renderer().getRenderData(0).drawGrid = userSettings.rendererDrawGrid;
    renderer.renderer().getRenderData(0).drawVectorField = userSettings.rendererDrawVectorField;
    renderer.renderer().getRenderData(0).drawFieldArrows = userSettings.rendererDrawFieldArrows;
    renderer.renderer().getRenderData(0).fieldAutoScale = userSettings.rendererFieldAutoScale;
    renderer.renderer().getRenderData(0).fieldPotentialScale = userSettings.rendererFieldPotentialScale;
    renderer.renderer().getRenderData(0).fieldCellSize = userSettings.rendererFieldCellSize;
    renderer.renderer().getRenderData(0).fieldSmoothing = userSettings.rendererFieldSmoothing;
    simulation.world().setVectorFieldCellSize(userSettings.rendererFieldCellSize);
    renderer.renderer().getRenderData(0).drawBonds = userSettings.rendererDrawBonds;
    renderer.renderer().getRenderData(0).drawBox = userSettings.rendererDrawBox;
    renderer.renderer().getRenderData(0).drawMemoryOrder = userSettings.rendererDrawMemoryOrder;
    renderer.renderer().getRenderData(0).speedColorMode = userSettings.rendererSpeedColorMode;
    renderer.renderer().getRenderData(0).speedGradientMax = userSettings.rendererSpeedGradientMax;
    simulation.world().getIntegrator().setScheme(userSettings.simulationIntegrator);
    simulation.world().setBondFormationEnabled(userSettings.simulationBondFormationEnabled);
    simulation.world().setLJEnabled(userSettings.simulationLJEnabled);
    simulation.world().setCoulombEnabled(userSettings.simulationCoulombEnabled);
    simulation.world().setCoulombLongRangeEnabled(userSettings.simulationCoulombLongRangeEnabled);
    appInterface.state().simulationSpeed = 100.0f;
    appInterface.state().pause = true;

    simulation.world().setVectorFieldSlice(static_cast<int>(simulation.world().getWorldSize().z * 0.5f));

    
    // создание сцены
    // Generators::triangularBipyramidCrystal(simulation, 8, AtomData::Type::H);
    // Generators::AngularVelocity(simulation, Vec3f(0.0f, 0.25f, 0.0f));
    // Generators::hexLattice(simulation, {5, 5, 1}, AtomData::Type::Z);
    
    // std::vector<Generators::AtomTypeSpec> gasSpecs = {
    //         // {AtomData::Type::O, 0, 80.0f},    // 80% водорода
    //         {AtomData::Type::Na, 0, 50.0f},   // 10% натрия
    //         {AtomData::Type::Cl, 0, 50.0f}    // 10% хлора
    //     };
    // Generators::randomGasMixed(simulation, 500, gasSpecs, false, 6.0, 6.0, 1.0f, 5.0f, 0);
    // simulation.createAtom(glm::vec3(20, 25, 3), glm::vec3(0, 0, 0), AtomData::Type::Na);
    // simulation.createAtom(glm::vec3(30, 25, 3), glm::vec3(0, 0, 0), AtomData::Type::Cl);
    renderer.syncScene(simulation);

    auto startTime = Clock::now();
    double renderAccum = 0.0;
    double physicsAccum = 0.0;
    double logAccum = 0.0;

    constexpr double renderInterval = 1.0 / FPS;
    constexpr double logInterval = 1.0 / LPS;

    renderer.setScreenSize(width, height);
    renderer.resetView();
    UiState& uiState = appInterface.state();

    presentInitializedWindow(window, renderer, simulation, appInterface, debugViews);

    while (!glfwWindowShouldClose(window)) {
        Profiler::instance().beginFrame();

        auto currentTime = Clock::now();
        const float deltaTime = std::chrono::duration<float>(currentTime - startTime).count();
        startTime = currentTime;

        physicsAccum += deltaTime;
        renderAccum += deltaTime;
        logAccum += deltaTime;

        EventManager::poll();
        if (exitRequested) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        if (glfwWindowShouldClose(window)) {
            break;
        }

        EventManager::frame(deltaTime);
        captureController.update(deltaTime);
        simulation.setXYZRecordingStepInterval(makeXYZStepInterval(uiState.simulationSpeed, captureController.settings().fps));
        captureController.syncUiState(uiState);
        uiState.xyzRecording = simulation.isXYZRecording();
        uiState.xyzFps = simulation.xyzFPS();
        uiState.xyzFrameCount = simulation.xyzFrameCount();
        captureController.handleToggleShortcut();

        // обновление физики
        const double physicsInterval = 1.0 / uiState.simulationSpeed;
        if (physicsAccum >= physicsInterval) {
            if (!uiState.pause) {
                appActions.updateSimulationStep(simulation);
                simulation.updateAll();
            }
            physicsAccum = 0.0;
        }

        // отрисовка кадра
        if (renderAccum >= renderInterval) {
            renderAccum -= renderInterval;
            renderer.renderFrame(simulation, appInterface, debugViews);
            if (exitRequested) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            } else {
                WindowController::processPendingToggle();
            }
        }

        Profiler::instance().endFrame();

        // обновление логов и данных счетчиков
        if (logAccum >= logInterval) {
            logAccum -= logInterval;
            refreshSimulationDebugViews(debugViews, simulation);
        }
    }

    captureController.stop();
    const UserSettings::WindowState windowState = WindowController::snapshot();
    UserSettingsIO::save(UserSettings{
        .captureOutputDirectory = captureController.outputDirectory(),
        .scenesDirectory = appInterface.scenesDirectory(),
        .captureSettings = captureController.settings(),
        .windowState = windowState,
        .rendererUse3D = renderer.renderer().camera.getMode() != Camera::Mode::Mode2D,
        .rendererDrawAtoms = renderer.renderer().getRenderData(0).drawAtoms,
        .rendererDrawGrid = renderer.renderer().getRenderData(0).drawGrid,
        .rendererDrawVectorField = renderer.renderer().getRenderData(0).drawVectorField,
        .rendererDrawFieldArrows = renderer.renderer().getRenderData(0).drawFieldArrows,
        .rendererFieldAutoScale = renderer.renderer().getRenderData(0).fieldAutoScale,
        .rendererDrawBonds = renderer.renderer().getRenderData(0).drawBonds,
        .rendererDrawBox = renderer.renderer().getRenderData(0).drawBox,
        .rendererDrawMemoryOrder = renderer.renderer().getRenderData(0).drawMemoryOrder,
        .interfaceScale = appInterface.uiScaleMultiplier(),
        .rendererSpeedColorMode = renderer.renderer().getRenderData(0).speedColorMode,
        .rendererSpeedGradientMax = renderer.renderer().getRenderData(0).speedGradientMax,
        .rendererFieldPotentialScale = renderer.renderer().getRenderData(0).fieldPotentialScale,
        .rendererFieldCellSize = renderer.renderer().getRenderData(0).fieldCellSize,
        .rendererFieldSmoothing = renderer.renderer().getRenderData(0).fieldSmoothing,
        .simulationIntegrator = simulation.world().getIntegrator().getScheme(),
        .simulationBondFormationEnabled = simulation.world().isBondFormationEnabled(),
        .simulationLJEnabled = simulation.isLJEnabled(),
        .simulationCoulombEnabled = simulation.isCoulombEnabled(),
        .simulationCoulombLongRangeEnabled = simulation.world().isCoulombLongRangeEnabled(),
    });
    appInterface.shutdown();
    return 0;
}
