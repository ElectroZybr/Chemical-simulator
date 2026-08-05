#include "UpdateDebugData.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "App/debug/CreateDebugPanels.h"
#include "App/interaction/ToolsManager.h"
#include "Lattice/Engine/Consts.h"
#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/metrics/MemoryMetrics.h"
#include "Lattice/Engine/metrics/Profiler.h"
#include "GUI/interface/panels/debug/view/DebugView.h"

namespace {
    double lastProfileSampleMs(const Profiler& profiler, std::initializer_list<const char*> names) {
        for (const char* name : names) {
            const double ms = profiler.lastActiveMs(name);
            if (ms > 0.0) {
                return ms;
            }
        }
        return 0.0;
    }
}

void updateAtomSelectionDebug(const DebugViews& debugViews, const Lattice::Simulation& simulation) {
    const AtomStorage& atoms = simulation.atoms();
    const auto& selectedAtomIds = ToolsManager::pickingSystem->getSelectedAtomIds();
    if (selectedAtomIds.size() == 1) {
        debugViews.atomSingle->visible = true;
        debugViews.atomBatch->visible = false;
        const size_t selectedIndex = atoms.indexOf(*selectedAtomIds.begin());
        if (selectedIndex < atoms.size()) {
            debugViews.atomSingle->add_data("Позиция", atoms.pos(selectedIndex));
            const float speed = glm::length(atoms.vel(selectedIndex));
            debugViews.atomSingle->add_data("Скорость (A/dt)", speed);
            debugViews.atomSingle->add_data("Скорость (м/с)", speed * Units::SpeedUnitToMps);
            debugViews.atomSingle->add_data("Скорость (км/ч)", speed * Units::SpeedUnitToKmph);
            debugViews.atomSingle->add_data("Силы", atoms.force(selectedIndex));
            debugViews.atomSingle->add_data("Пред. силы", atoms.prevForce(selectedIndex));
            debugViews.atomSingle->add_data("Потенциальная энергия", atoms.energy()[selectedIndex]);
            const AtomData::Type atomType = atoms.type()[selectedIndex];
            debugViews.atomSingle->add_data("Масса", AtomData::getProps(atomType).mass);
            debugViews.atomSingle->add_data("Радиус", AtomData::getProps(atomType).radius);
            debugViews.atomSingle->add_data("Тип", static_cast<int>(atomType));
            debugViews.atomSingle->add_data("Валентность", atoms.valence()[selectedIndex]);
            std::string hybrid_name;
            uint8_t hybridization = atoms.hybridization()[selectedIndex];
            switch (hybridization) {
            case 1:
                hybrid_name = "S";
                break;
            case 2:
                hybrid_name = "SP";
                break;
            case 3:
                hybrid_name = "SP2";
                break;
            case 4:
                hybrid_name = "SP3";
                break;
            case 5:
                hybrid_name = "SP3D";
                break;
            case 6:
                hybrid_name = "SP3D2";
                break;
            case 7:
                hybrid_name = "SP3D3";
                break;
            default:
                hybrid_name = "None";
                break;
            }
            debugViews.atomSingle->add_data("Гибридизация", hybrid_name);
        }
    }
    else {
        debugViews.atomBatch->visible = true;
        debugViews.atomSingle->visible = false;
        debugViews.atomBatch->add_data("Выбрано атомов", selectedAtomIds.size());
    }
}

void updateSimulationDebug(const DebugViews& debugViews, const Lattice::Simulation& simulation, std::string_view integratorName) {
    struct StepsRateSample {
        int lastStep = 0;
        std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
        float rate = 0.0f;
    };

    static StepsRateSample stepsRateSample{};

    const AtomStorage& atoms = simulation.atoms();
    const World& world = simulation.world();
    const NeighborList& neighborList = simulation.neighborList();
    const Profiler& profiler = Profiler::instance();
    const double renderMs = lastProfileSampleMs(profiler, {"SceneViewport::renderFrame", "Application::RenderFrame"});
    const double captureReadbackMs = lastProfileSampleMs(profiler, {"Capture::readback", "FrameProducer::onBufferMapped"});
    const double captureEncodeMs = lastProfileSampleMs(profiler, {"Capture::encodeFrame", "FFmpegStreamer::writeFrame"});
    const double physicsMs =
        std::max({profiler.lastActiveMs("Simulation::update"), profiler.lastActiveMs("Simulation::updateAll"),
                  profiler.lastActiveMs("Simulation::updateWorld")});
    const int simStep = simulation.world().getSimStep();
    const auto now = std::chrono::steady_clock::now();
    const float elapsedSeconds = std::chrono::duration<float>(now - stepsRateSample.lastTime).count();
    if (elapsedSeconds >= 0.25f) {
        stepsRateSample.rate = elapsedSeconds > 0.0f ? static_cast<float>(simStep - stepsRateSample.lastStep) / elapsedSeconds : 0.0f;
        stepsRateSample.lastStep = simStep;
        stepsRateSample.lastTime = now;
    }
    const float stepsPerSecond = stepsRateSample.rate;

    debugViews.sim->add_data("Средняя скорость (км/ч)", simulation.world().getMetrics().averageSpeedKmPerHour());
    debugViews.sim->add_data("Полная энергия (pj)", simulation.world().getMetrics().fullAverageEnergyEv() * simulation.atoms().size() * Units::kEvToPJ);
    debugViews.sim->add_data("Полная средняя энергия (eV)", simulation.world().getMetrics().fullAverageEnergyEv());
    debugViews.sim->add_data("Температура (K)", simulation.world().getMetrics().temperatureK());
    debugViews.sim->add_data("Температура (°C)", simulation.world().getMetrics().temperatureC());
    debugViews.sim->add_data("Память (МБ)", MemoryMetrics::getRSS() / 1024.f / 1024.f);
    debugViews.sim->add_data("Рендер (мс)", renderMs);
    debugViews.sim->add_data("Capture readback (ms)", captureReadbackMs);
    debugViews.sim->add_data("Capture encode (ms)", captureEncodeMs);
    debugViews.sim->add_data("Физика (мс)", physicsMs);
    debugViews.sim->add_data("Количество атомов", atoms.size());
    debugViews.sim->add_data("Шаги симуляции", simStep);
    debugViews.sim->add_data("Шагов/с", stepsPerSecond);
    debugViews.sim->add_data("Время симуляции (ns)", simulation.simTimeNs());
    debugViews.sim->add_data("Тип интегратора", integratorName);

    const std::string gridSize =
        std::format("{} x {} x {}", std::max<long long>(0, world.getGrid().size.x - 2), std::max<long long>(0, world.getGrid().size.y - 2),
                    std::max<long long>(0, world.getGrid().size.z - 2));
    debugViews.neighbor->add_data("Размер сетки", gridSize);
    const std::string boxSizeNm = std::format("{:.2f} x {:.2f} x {:.2f}", world.getWorldSize().x * Units::AngstromToNm,
                                              world.getWorldSize().y * Units::AngstromToNm, world.getWorldSize().z * Units::AngstromToNm);
    debugViews.neighbor->add_data("Размер бокса (nm)", boxSizeNm);
    debugViews.neighbor->add_data("Размер ячейки", world.getGrid().cellSize);
    debugViews.neighbor->add_data("NeighborList включен", std::string("Да"));
    debugViews.neighbor->add_data("Память AtomStorage (МБ)", static_cast<float>(atoms.memoryBytes()) / 1024.0f / 1024.0f);
    debugViews.neighbor->add_data("Память NeighborList (МБ)", static_cast<float>(neighborList.memoryBytes()) / 1024.0f / 1024.0f);
    debugViews.neighbor->add_data("Память SpatialGrid (МБ)", static_cast<float>(world.getGrid().memoryBytes()) / 1024.0f / 1024.0f);
    debugViews.neighbor->add_data("Пар в NL", neighborList.pairStorageSize());
    debugViews.neighbor->add_data("Ср. соседей на атом", neighborList.stats().avgNeighborsPerAtom(neighborList));
    debugViews.neighbor->add_data("Cutoff", neighborList.cutoff());
    debugViews.neighbor->add_data("Skin", neighborList.skin());
    debugViews.neighbor->add_data("List radius", neighborList.listRadius());
    debugViews.neighbor->add_data("Ребилдов NL", neighborList.stats().rebuildCount());
    debugViews.neighbor->add_data("Шагов между ребилдами (recent)", neighborList.stats().recentAverageStepsBetweenRebuilds());
    debugViews.neighbor->add_data("Время ребилда NL (мс)", neighborList.stats().lastRebuildTimeMs());
    debugViews.neighbor->add_data("SG заполненных ячеек", static_cast<int>(world.getGrid().stats().lastNonEmptyCellCount()));
    debugViews.neighbor->add_data("SG макс атомов в ячейке", static_cast<int>(world.getGrid().stats().lastMaxAtomsPerCell()));
    debugViews.neighbor->add_data("SG ср. атомов/ячейку", world.getGrid().stats().lastAverageAtomsPerNonEmptyCell());
}
