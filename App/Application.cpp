#include "Application.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "App/AppActions.h"
#include "App/AppIOSystem/MoleculeTemplatesIO.h"
#include "App/AppIOSystem/UserSettings.h"
#include "App/WindowController.h"
#include "Lattice/Scripting/LuaState.h"
#include "Lattice/Log.hpp"
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
#include "Lattice/Engine/pluginLoader.hpp"

#include "Lattice/Kernel/Runtime.hpp"
#include "Lattice/Kernel/Universe.hpp"

using Clock = std::chrono::high_resolution_clock;

constexpr int FPS = 60;
constexpr int LPS = 20;

namespace {
    const std::filesystem::path MoleculesPath = std::filesystem::path("Mods") / "Base" / "Molecules";
    const std::filesystem::path pluginsPath = std::filesystem::path("Plugins");

    uint32_t makeXYZStepInterval(float simulationStepsPerSecond, int captureFps) {
        const float sanitizedStepsPerSecond = std::max(simulationStepsPerSecond, 1.0f);
        const int sanitizedCaptureFps = std::max(captureFps, 1);
        return std::max(1u, static_cast<uint32_t>(std::lround(sanitizedStepsPerSecond / static_cast<float>(sanitizedCaptureFps))));
    }
}

int Application::run() {
    PluginLoader pluginLoader;
    pluginLoader.load(pluginsPath);
    
    Lattice::Runtime runtime;
    runtime.loadPlugins(pluginsPath);
    Lattice::Universe& universe = runtime.createUniverse();
    universe.configure("ClassicMD");
    universe.update();

    UserSettings userSettings;
    {
        LogScope applicationScope("Application", "LatticeLab starting...", "Initialized");

        userSettings = UserSettingsIO::load();

        GLFWwindow* window = nullptr;
        {
            window = WindowController::create(userSettings.windowState);
            if (!window) {
                applicationScope.cancel();
                Log::fatal("Window", "Failed to create main window");
                return EXIT_FAILURE;
            }
        }

        const auto [width, height] = WindowController::framebufferSize();
        {
            LogScope rendererScope("Renderer", "Initializing", "Renderer initialized");
            rendererScope.step("Creating WebGPU context");
            WGPUContext::instance().init(window, width, height);
        }

        Lattice::Simulation simulation;
        {
            LogScope simulationScope("Simulation", "Initializing", "Simulation initialized");
            simulationScope.step("Loading molecule templates");
            MoleculeTemplatesIO::loadFromDirectory(simulation, MoleculesPath);
            simulationScope.step("Creating world");
            simulation.createWorld(glm::vec3(120.0f, 120.0f, 10.0f));
        }
        Lattice::LuaState luaState;
        luaState.bindSimulation(simulation);

        CaptureController captureController;
        SceneViewport renderer(userSettings, captureController);
        renderer.syncScene(simulation);

        Interface appInterface(window, simulation, renderer.rendererHandle(), captureController);
        appInterface.toolsPanel.setRendererType(renderer.rendererType());

        AppActions::Handler appActions(window, captureController, simulation, renderer, appInterface.state());
        CaptureActions::Handler captureActions(captureController);
        {
            LogScope interfaceScope("Interface", "Initializing", "Interface initialized");
            interfaceScope.step("Creating ImGui context");
            if (appInterface.init() != EXIT_SUCCESS) {
                interfaceScope.cancel();
                applicationScope.cancel();
                Log::fatal("Interface", "Failed to initialize application interface");
                return EXIT_FAILURE;
            }
        }
        EventManager::init(window, renderer.rendererHandle(), appInterface);
        ToolsManager::init(window, simulation, renderer.rendererHandle(), appInterface);
        const DebugViews debugViews = createDebugViews(appInterface.debugPanel);

        userSettings.applyTo(captureController);
        userSettings.applyTo(appInterface);
        userSettings.applyTo(renderer);
        userSettings.applyTo(simulation);

        simulation.world().setVectorFieldSlice(static_cast<int>(simulation.world().getWorldSize().z * 0.5f));

        renderer.syncScene(simulation);

        auto startTime = Clock::now();
        double renderAccum = 0.0;
        double physicsAccum = 0.0;
        double debugRefreshAccum = 0.0;
        double statusAccum = 0.0;
        uint64_t lastStatusStep = simulation.getSimStep();

        constexpr double renderInterval = 1.0 / FPS;
        constexpr double debugRefreshInterval = 1.0 / LPS;
        constexpr double statusInterval = 2.0;

        renderer.setScreenSize(width, height);
        renderer.resetView();
        UiState& uiState = appInterface.state();

        renderer.renderFrame(simulation, appInterface, debugViews);
        glfwShowWindow(window);
        glfwFocusWindow(window);
        applicationScope.finish();

        while (!glfwWindowShouldClose(window)) {
            Profiler::instance().beginFrame();

            auto currentTime = Clock::now();
            const float deltaTime = std::chrono::duration<float>(currentTime - startTime).count();
            startTime = currentTime;

            physicsAccum += deltaTime;
            renderAccum += deltaTime;
            debugRefreshAccum += deltaTime;
            statusAccum += deltaTime;

            EventManager::poll();
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
            }

            Profiler::instance().endFrame();

            // обновление debug-счетчиков и служебных панелей
            if (debugRefreshAccum >= debugRefreshInterval) {
                debugRefreshAccum -= debugRefreshInterval;
                refreshSimulationDebugViews(debugViews, simulation);
            }

            // периодический статус приложения в логах
            if (statusAccum >= statusInterval) {
                const uint64_t currentStep = simulation.getSimStep();
                const uint64_t advancedSteps = currentStep - lastStatusStep;
                const double averageUps = advancedSteps / statusAccum;

                Log::info(
                    "Simulation",
                    "Status: step={} atoms={} paused={} speed={} UPS={:.1f}",
                    currentStep,
                    simulation.world().getAtomStorage().size(),
                    uiState.pause,
                    uiState.simulationSpeed,
                    averageUps);

                lastStatusStep = currentStep;
                statusAccum = 0.0;
            }
        }

        Log::action("Application", "Shutting down...");
        captureController.stop();
        const UserSettings::WindowState windowState = WindowController::snapshot();
        UserSettings userSettings;
        userSettings.captureFrom(captureController);
        userSettings.captureFrom(appInterface);
        userSettings.captureFrom(renderer);
        userSettings.captureFrom(simulation);
        userSettings.captureFrom(windowState);
        UserSettingsIO::save(userSettings);
        appInterface.shutdown();
        Log::ok("Application", "Shutdown complete");
        return 0;
    }
}
