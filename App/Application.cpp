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

    void presentInitializedWindow(GLFWwindow* window, SceneViewport& renderer, const Lattice::Simulation& simulation, Interface& appInterface,
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

    AppActions::Handler appActions(window, captureController, simulation, renderer, appInterface.state());
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

    
    // Стартовая сцена: небольшой вращающийся кристалл — демо при запуске (другие сцены
    // грузятся через IO-панель). Без него первый экран пустой.
    Generators::triangularBipyramidCrystal(simulation, 8, AtomData::Type::Z);
    Generators::AngularVelocity(simulation, glm::vec3(0.0f, 0.25f, 0.0f));
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
        EventManager::frame(deltaTime);
        captureController.update(deltaTime);
        simulation.setXYZRecordingStepInterval(makeXYZStepInterval(uiState.simulationSpeed, captureController.settings().fps));
        captureController.syncUiState(uiState);
        uiState.xyzRecording = simulation.isXYZRecording();
        uiState.xyzFps = simulation.xyzFPS();
        uiState.xyzFrameCount = simulation.xyzFrameCount();
        captureController.handleToggleShortcut();

        // обновление физики (fixed-timestep accumulator)
        const double physicsInterval = 1.0 / uiState.simulationSpeed;
        if (uiState.pause) {
            // На паузе не копим долг — иначе при unpause последует лавина шагов.
            physicsAccum = 0.0;
        }
        else {
            // anti-spiral: если уже сильно отстаём (тяжёлая сцена / просадка),
            // выкидываем избыток времени вместо бесконечного догоняния.
            constexpr int kMaxStepsPerFrame = 8;
            const double maxAccum = kMaxStepsPerFrame * physicsInterval;
            if (physicsAccum > maxAccum) {
                physicsAccum = maxAccum;
            }
            int stepsThisFrame = 0;
            while (physicsAccum >= physicsInterval && stepsThisFrame < kMaxStepsPerFrame) {
                simulation.updateAll();
                physicsAccum -= physicsInterval;
                ++stepsThisFrame;
            }
        }

        // отрисовка кадра
        if (renderAccum >= renderInterval) {
            renderAccum -= renderInterval;

            // --- GPU-режим: условный per-frame sync ПЕРЕД renderFrame ---
            // Апстрим вынес тело кадра в SceneViewport::renderFrame(const Simulation&):
            // внутри он зовёт appInterface.update() (там UI-тумблеры применяются) и
            // syncRendererWithSimulation (читает CPU-позиции). syncFromGpuIfNeeded /
            // refreshDiagnosticsGrid МУТИРУЮТ Simulation, поэтому в const-renderFrame им
            // места нет — делаем их здесь, где simulation ещё mutable, ДО renderFrame.
            //
            // Overlay соседей рисует грид-обход только при РОВНО одном выбранном атоме
            // (NeighborListOverlay::draw), поэтому проверка == 1.
            const bool singleSelection =
                ToolsManager::pickingSystem != nullptr && ToolsManager::pickingSystem->getSelectedAtomIds().size() == 1;

            // Render-настройки живут пер-мир в BaseRenderer::getRenderData(worldId). UI пишет в
            // активный мир, но для sync-предиката берём ИЛИ по ВСЕМ мирам: при мультимире
            // неактивный GPU-мир с bonds/grid тоже рендерится и не должен читать устаревшие CPU-
            // данные. Для одного мира цикл из 1 итерации = флаги активного мира — поведение
            // прежнее, zero-copy single-world не меняется.
            bool drawBonds = false;
            bool drawGrid = false;
            bool speedColorAutoMax = false;
            const size_t renderDataCount = renderer.renderer().getRenderDataCount();
            for (size_t w = 0; w < renderDataCount; ++w) {
                const RenderData& rd = renderer.renderer().getRenderData(w);
                drawBonds = drawBonds || rd.drawBonds;
                drawGrid = drawGrid || rd.drawGrid;
                // speed-color АВТО-max: рендер CPU-сканит max|vel| по AtomStorage для нормировки.
                // При заданном вручную speedGradientMax > 0 скан НЕ делается → синк не нужен.
                speedColorAutoMax = speedColorAutoMax ||
                    (rd.speedColorMode != RenderData::SpeedColorMode::AtomColor && rd.speedGradientMax <= 0.0f);
            }

            // Инкремент B (zero-copy): атомы рендерятся ПРЯМО из резидентных VRAM-буферов
            // (Инкремент A), поэтому per-frame download нужен НЕ всегда, а только если
            // активен хоть один CPU-потребитель позиций/скоростей. Перечисляем их явно:
            //  - drawBonds  — связи рисуются по CPU-позициям;
            //  - drawGrid / singleSelection — viz-сетка и overlay соседей читают CPU-грид
            //    (через refreshDiagnosticsGrid ниже, у которого precondition «позиции синканы»);
            //  - debugPanel видна — atom/sim debug-views читают CPU pos/vel (UpdateDebugData);
            //  - speed-color АВТО-max — рендер CPU-сканит max|vel| по AtomStorage.
            // Если НИ ОДИН не активен («чистый» GPU-режим: атомы only, atom-color) —
            // download ПРОПУСКАЕТСЯ целиком, атомы рисуются zero-copy. Это и есть выигрыш.
            // Консервативно: при любом сомнении синк делается (лишний download безопасен,
            // syncFromGpuIfNeeded идемпотентен по cpuPositionsDirty). В CPU-режиме — no-op.
            // Флаги выше — ИЛИ по ВСЕМ мирам (не только активному), поэтому download включается,
            // если хоть один мир (в т.ч. неактивный) имеет CPU-потребителя позиций (bonds/grid/
            // speed-auto/debug). Атомы рисуются zero-copy из VRAM для ВСЕХ миров — отдельный
            // per-frame download для самих атомов не нужен (в этом и выигрыш).
            const bool cpuPositionConsumerActive =
                drawBonds || drawGrid || singleSelection || appInterface.debugPanel.isVisible() || speedColorAutoMax;
            if (cpuPositionConsumerActive) {
                simulation.syncFromGpuIfNeeded();
            }

            // В GPU-режиме CPU SpatialGrid застывает (hot loop перестраивает NL на GPU и
            // грид не трогает), а его читают диагностические потребители: визуализация
            // сетки (drawGrid), overlay соседей выбранного атома и debug-панель статистики
            // грида. Перебинниваем грид из УЖЕ синканных позиций ТОЛЬКО когда хоть один из
            // них активен. gridConsumerActive ⊆ cpuPositionConsumerActive, поэтому
            // syncFromGpuIfNeeded уже вызван — precondition refreshDiagnosticsGrid соблюдён.
            const bool gridConsumerActive = drawGrid || singleSelection || appInterface.debugPanel.isVisible();
            if (gridConsumerActive) {
                // refreshDiagnosticsGrid сам пропускает не-GPU миры и перебиннивает грид
                // ВСЕХ GPU-миров (мульти-мир контракт), проверка isGpuMode здесь не нужна.
                simulation.refreshDiagnosticsGrid();
            }

            renderer.renderFrame(simulation, appInterface, debugViews);
        }

        Profiler::instance().endFrame();

        // обновление логов и данных счетчиков
        if (logAccum >= logInterval) {
            logAccum -= logInterval;
            // refreshSimulationDebugViews читает velocity-метрики (averageSpeed/temperature/
            // energy через metricsCache_ из AtomStorage) на ЛОГ-каденции — отдельной от
            // рендера. В GPU-режиме при УСЛОВНОМ синке (Инкремент B) эти CPU pos/vel свежи
            // только когда сделан per-frame download. Гейтим refresh на видимости debug-
            // панели: если она видна — она же в cpuPositionConsumerActive, значит синк на
            // каденции рендера (60 Гц) опережает этот лог-refresh (20 Гц), и метрики свежи;
            // если скрыта — refresh пропускается, чтобы НЕ читать устаревший снимок и не
            // тратить O(N) на метрику, которую всё равно никто не показывает (§11 edit 5).
            if (appInterface.debugPanel.isVisible()) {
                refreshSimulationDebugViews(debugViews, simulation);
            }
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
        .rendererDrawBonds = renderer.renderer().getRenderData(0).drawBonds,
        .rendererDrawBox = renderer.renderer().getRenderData(0).drawBox,
        .rendererDrawMemoryOrder = renderer.renderer().getRenderData(0).drawMemoryOrder,
        .interfaceScale = appInterface.uiScaleMultiplier(),
        .rendererSpeedColorMode = renderer.renderer().getRenderData(0).speedColorMode,
        .rendererSpeedGradientMax = renderer.renderer().getRenderData(0).speedGradientMax,
        .simulationIntegrator = simulation.world().getIntegrator().getScheme(),
        .simulationBondFormationEnabled = simulation.world().isBondFormationEnabled(),
        .simulationLJEnabled = simulation.isLJEnabled(),
        .simulationCoulombEnabled = simulation.isCoulombEnabled(),
        .simulationCoulombLongRangeEnabled = simulation.world().isCoulombLongRangeEnabled(),
    });
    appInterface.shutdown();
    return 0;
}
