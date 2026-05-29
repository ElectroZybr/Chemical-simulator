#include "Application.h"

#include <algorithm>
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
#include "Rendering/3d/Renderer3DWGPU.h"
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
    Simulation simulation;

    simulation.createWorld({120, 120, 120});

    CaptureController captureController;
    std::unique_ptr<IRenderer> renderer = std::make_unique<Renderer3DWGPU>(simulation.world(), WGPUContext::instance().surfaceFormat());
    Interface appInterface(window, simulation, renderer, captureController);
    appInterface.toolsPanel.setRendererType(renderer->camera.getMode() == Camera::Mode::Mode2D ? RendererType::Renderer2D : RendererType::Renderer3D);
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
    renderer->drawBox = userSettings.rendererDrawBox;
    renderer->speedColorMode = userSettings.rendererSpeedColorMode;
    renderer->speedGradientMax = userSettings.rendererSpeedGradientMax;
    simulation.setIntegrator(userSettings.simulationIntegrator);
    simulation.setBondFormationEnabled(userSettings.simulationBondFormationEnabled);
    simulation.setLJEnabled(userSettings.simulationLJEnabled);
    simulation.setCoulombEnabled(userSettings.simulationCoulombEnabled);
    appInterface.state().simulationSpeed = 100.0f;
    appInterface.state().pause = true;

    // создание сцены
    Scenes::triangularBipyramidCrystal(simulation, 8, AtomData::Type::Z);
    Scenes::AngularVelocity(simulation, Vec3f(0.0f, 0.25f, 0.0f));

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

    renderer->camera.setScreenSize({static_cast<float>(width), static_cast<float>(height)});
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
            PROFILE_SCOPE("Application::RenderFrame");
            renderAccum -= renderInterval;

            // UI обновляем ПЕРВЫМ в кадре: SettingsPanel может тут переключить
            // drawBonds/drawGrid/speedColor, а от них зависит cpuPositionConsumerActive
            // и решение о per-frame sync ниже. Если считать предикат ДО update(),
            // включение потребителя из «чистого» режима дало бы один stale-кадр (sync
            // решён по старым флагам, а drawShot ниже уже рисует с новыми).
            uiState.simStep = simulation.getSimStep();
            appInterface.update();

            // Overlay соседей рисует грид-обход только при РОВНО одном выбранном
            // атоме (NeighborListOverlay::draw), поэтому здесь проверка == 1.
            const bool singleSelection = ToolsManager::pickingSystem && ToolsManager::pickingSystem->getSelectedIndices().size() == 1;

            // Инкремент B (zero-copy): атомы рендерятся ПРЯМО из резидентных VRAM-буферов
            // (Инкремент A), поэтому per-frame download нужен НЕ всегда, а только если
            // активен хоть один CPU-потребитель позиций/скоростей. Перечисляем их явно:
            //  - drawBonds  — связи рисуются по CPU-позициям (RendererWGPU::drawBondsImpl);
            //  - drawGrid / singleSelection — viz-сетка и overlay соседей читают CPU-грид
            //    (через refreshDiagnosticsGrid ниже, у которого precondition «позиции синканы»);
            //  - debugPanel видна — atom/sim debug-views читают CPU pos/vel (UpdateDebugData);
            //  - speed-color АВТО-max (speedColorMode != AtomColor && speedGradientMax <= 0)
            //    — рендер CPU-сканит max|vel| по AtomStorage для нормировки (R3/§11 edit 6);
            //    при заданном вручную speedGradientMax > 0 скан НЕ делается → синк не нужен.
            // Если НИ ОДИН не активен («чистый» GPU-режим: атомы only, atom-color) —
            // download ПРОПУСКАЕТСЯ целиком, атомы рисуются zero-copy. Это и есть выигрыш.
            // Консервативно: при любом сомнении синк делается (лишний download безопасен,
            // syncFromGpuIfNeeded идемпотентен по cpuPositionsDirty). В CPU-режиме — no-op.
            const bool speedColorAutoMax =
                renderer->speedColorMode != IRenderer::SpeedColorMode::AtomColor && renderer->speedGradientMax <= 0.0f;
            const bool cpuPositionConsumerActive =
                renderer->drawBonds || renderer->drawGrid || singleSelection || appInterface.debugPanel.isVisible() || speedColorAutoMax;
            if (cpuPositionConsumerActive) {
                simulation.syncFromGpuIfNeeded();
            }

            // В GPU-режиме CPU SpatialGrid застывает (hot loop перестраивает NL на
            // GPU и грид не трогает), а его читают диагностические потребители:
            // визуализация сетки (drawGrid), overlay соседей выбранного атома и
            // debug-панель статистики грида. Перебинниваем грид из УЖЕ синканных
            // позиций ТОЛЬКО когда хоть один из них активен — иначе зря тратим O(N)
            // на каждый кадр. App знает про UI-тумблеры; движок про них не знает.
            // gridConsumerActive ⊆ cpuPositionConsumerActive, поэтому к этому месту
            // syncFromGpuIfNeeded уже вызван — precondition refreshDiagnosticsGrid
            // (позиции синканы) соблюдён.
            const bool gridConsumerActive = renderer->drawGrid || singleSelection || appInterface.debugPanel.isVisible();
            if (gridConsumerActive) {
                // Без проверки isGpuMode() активного мира: рендер рисует и неактивные
                // GPU-миры, а refreshDiagnosticsGrid сам пропускает не-GPU миры и
                // перебиннивает грид ВСЕХ GPU-миров (мульти-мир контракт).
                simulation.refreshDiagnosticsGrid();
            }

            refreshAtomDebugViews(debugViews, simulation);

            WGPUContext& ctx = WGPUContext::instance();

            // получаем surface текстуру один раз на кадр
            wgpu::SurfaceTexture surfaceTex;
            ctx.surface()->getCurrentTexture(&surfaceTex);
            wgpu::raii::Texture surfaceTexture(surfaceTex.texture);
            wgpu::raii::TextureView surfaceView = surfaceTexture->createView();

            // - нет захвата -> возвращает view от surface напрямую
            // - идёт захват -> возвращает view intermediate текстуры
            wgpu::TextureView renderTarget = captureController.acquireRenderTarget(*surfaceTexture, *surfaceView);

            renderer->drawShot(renderTarget, *ctx.depthView(), simulation);
            ToolsManager::overlay.draw();
            ImGui::Render();
            auto* wgpuRenderer = static_cast<RendererWGPU*>(renderer.get());
            ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), *wgpuRenderer->getCurrentPass());
            renderer->endFrame();

            // захват кадра для видео
            captureController.onFrameRendered(*surfaceTexture);

            ctx.present();
            ctx.processEvents();
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
    UserSettingsIO::save(UserSettings{
        .captureOutputDirectory = captureController.outputDirectory(),
        .scenesDirectory = appInterface.scenesDirectory(),
        .captureSettings = captureController.settings(),
        .rendererDrawGrid = renderer->drawGrid,
        .rendererDrawBonds = renderer->drawBonds,
        .rendererDrawBox = renderer->drawBox,
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
