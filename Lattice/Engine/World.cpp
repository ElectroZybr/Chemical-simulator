#include "World.h"

#include "Engine/metrics/EnergyMetrics.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/Integrator.h"
#include "Engine/physics/gpu/GpuResidentPhysics.h"

World::World(glm::vec3 size, glm::vec3 renderOffset) : size(size), renderOffset(renderOffset), grid(size) {
    atomStorage_.reserve(250000);
    neighborList_.setParams(5.f, 1.f);
#ifdef LATTICELAB_USE_TBB
    // Auto-mode: на каждом rebuild NL выбирает Half для mobileCount<5000
    // (избегаем 2x работу force loop на маленьких сценах) и Full выше
    // (parallel-выгода окупает удвоенную NL).
    neighborList_.setAutoMode(5000);
#endif
}

// Out-of-line: GpuResidentPhysics — неполный тип в World.h. Дефолтные деструктор/
// move-операции должны генерироваться там, где тип полный (этот .cpp).
World::~World() = default;
World::World(World&&) noexcept = default;
World& World::operator=(World&&) noexcept = default;

void World::clear() {
    // Сцена сбрасывается целиком — старое состояние VRAM не нужно (и качать его
    // перед очисткой нельзя: затёрло бы свежезагружаемые атомы). Версия растёт,
    // чтобы пустая/новая сцена была залита в GPU на ближайшем шаге.
    cpuPositionsDirty_ = false;
    clearAtoms();
    clearBonds();
    neighborList_.clear();
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
    invalidateMetrics();
    ++cpuSceneVersion_;
}

void World::reset() {
    clear();
    clearMetadata();
    resetRuntimeState();
}

void World::resizeBox(const glm::vec3& newSize, float cellSize) {
    syncGpuBeforeEdit(); // grid rebuild ниже должен видеть актуальные позиции
    setWorldSize(newSize);
    setGridCellSize(cellSize);
    finalizeAtomBatch();
    notifySceneEdited(); // worldMax_/grid в VRAM устарели — нужен re-upload
}

void World::addAtom(const glm::vec3& start_coords, const glm::vec3& start_speed, AtomData::Type type, bool fixed) {
    syncGpuBeforeEdit(); // подтянуть свежие позиции существующих атомов перед добавлением
    atomStorage_.addAtom(start_coords, start_speed, type, fixed);
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
    invalidateMetrics();
    notifySceneEdited();
}

void World::addBond(size_t aIndex, size_t bIndex) {
    const Bond* created = Bond::CreateBond(bonds_, aIndex, bIndex, atomStorage_);
    if (created == nullptr) {
        // Связь НЕ создана (невалидные индексы / само-связь / исчерпана валентность /
        // дубликат / нулевые params): топология не изменилась — бампить версию и гнать
        // дорогой re-upload bond-CSR незачем.
        return;
    }
    // Связь добавлена → топология изменилась. Бамп версии сцены заставит ближайший
    // updateGpu перезалить bond-CSR в VRAM (uploadSceneToGpu → uploadBonds), иначе
    // добавленная в GPU-режиме связь молча не получала бы Morse-силы. addBond не
    // трогает позиции, поэтому syncGpuBeforeEdit не нужен (re-upload несёт текущие
    // VRAM-позиции через downloadToCpu при cpuPositionsDirty_).
    notifySceneEdited();
}

void World::remapAtomIndices(std::span<const uint32_t> oldToNew) {
    if (oldToNew.empty()) {
        return;
    }

    for (Bond& bond : bonds_) {
        if (bond.aIndex < oldToNew.size()) {
            bond.aIndex = oldToNew[bond.aIndex];
        }
        if (bond.bIndex < oldToNew.size()) {
            bond.bIndex = oldToNew[bond.bIndex];
        }
    }
}

void World::removeAtom(size_t atomIndex) {
    if (atomIndex >= atomStorage_.size()) {
        return;
    }

    syncGpuBeforeEdit();

    const size_t lastIndex = atomStorage_.size() - 1;

    for (auto it = bonds_.begin(); it != bonds_.end();) {
        if (it->aIndex == atomIndex || it->bIndex == atomIndex) {
            if (it->aIndex == atomIndex && it->bIndex != atomIndex && it->bIndex < atomStorage_.size()) {
                ++atomStorage_.valenceCount(it->bIndex);
            }
            if (it->bIndex == atomIndex && it->aIndex != atomIndex && it->aIndex < atomStorage_.size()) {
                ++atomStorage_.valenceCount(it->aIndex);
            }
            it = bonds_.erase(it);
            continue;
        }

        if (atomIndex != lastIndex) {
            if (it->aIndex == lastIndex) {
                it->aIndex = atomIndex;
            }
            if (it->bIndex == lastIndex) {
                it->bIndex = atomIndex;
            }
        }

        ++it;
    }

    atomStorage_.removeAtom(atomIndex);
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
    invalidateMetrics();
    notifySceneEdited();
}

void World::finalizeAtomBatch() {
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
    neighborList_.clear();
    invalidateMetrics();
    notifySceneEdited(); // батч мог изменить контент при том же числе атомов (load той же длины)
}

const EnergyMetrics::Snapshot& World::getMetrics() const {
    if (state_.metricsCacheValid_) {
        return state_.metricsCache_;
    }

    state_.metricsCache_ = EnergyMetrics::buildSnapshot(atomStorage_);
    state_.metricsCacheValid_ = true;
    return state_.metricsCache_;
}

void World::update() {
    if (gpu_) {
        updateGpu();
        return;
    }

    // Перестроить список соседей если необходимо
    if (neighborList_.needsRebuild(atomStorage_)) {
        neighborList_.rebuildPipeline(atomStorage_, *this, state_.sim_step);
    }

    // Создать данные для шага
    StepData stepData{
        .world = *this,
        .forceField = state_.forceField_,
        .neighborList = neighborList_,
        .allowBondFormation = state_.bondFormationEnabled_,
        .bondsChanged = false,
        .accelDamping = state_.integrator.accelDamping(),
        .dt = state_.Dt,
    };

    // Выполнить шаг интеграции
    state_.integrator.step(stepData);

    // Обновить счётчики и время
    state_.metricsCacheValid_ = false;
    ++state_.sim_step;
    state_.sim_time_ns += state_.Dt * Units::kTimeUnitToNs;
}

void World::updateGpu() {
    // Сцена могла измениться под резидентным GPU (добавили/удалили атомы, загрузили
    // сцену): VRAM-буфера залиты под старый totalCount — путь NL rebuild переполнил
    // бы их (writeBuffer overrun), а новые атомы вообще не интегрировались бы.
    // Замечаем расхождение по счётчику атомов И по версии сцены и заново заливаем
    // активную сцену (буфера растут в ensureCapacity). CPU-копия к моменту правки
    // свежа: обработчики правки сцены синкают GPU→CPU перед собой (syncGpuBeforeEdit),
    // а uploadSceneToGpu при cpuPositionsDirty_ сливает прогресс GPU перед
    // перезаливкой.
    if (atomStorage_.size() != gpu_->totalCount() || cpuSceneVersion_ != gpuUploadedSceneVersion_) {
        uploadSceneToGpu();
    }

    // NL-rebuild решается по GPU-редукции смещения АСИНХРОННО: редукция запускается,
    // результат читается неблокирующе на следующих шагах; rebuild по готовому (на
    // 1-2 шага устаревшему) значению. Hard backstop: pending старше
    // kDispReadbackMaxLagSteps дожидается форс-синком ПЕРЕД шагом — безопасная
    // граница для mobile-mobile пар именно 0.5*skin (два атома идут навстречу),
    // поэтому latency нельзя отпускать.
    constexpr int kDispCheckCadence = 4;
    constexpr int kDispReadbackMaxLagSteps = 2;
    const float skin = neighborList_.skin();
    const float threshold = 0.5f * skin;
    const float thresholdSqr = threshold * threshold;

    auto rebuildNeighborList = [&]() {
        // Шаг 2d: NL пересобирается ЦЕЛИКОМ на GPU из резидентных позиций — БЕЗ CPU
        // round-trip. Параметры сетки берём из CPU SpatialGrid (он не перестраивается
        // в hot loop, но его size/cellSize/countCells валидны с момента конфигурации/
        // входа в GPU-режим и совпадают с биннингом, который ждёт GPU-builder). refPos
        // обновляется внутри rebuildNeighborListOnGpu. CPU-копия NL при этом легитимно
        // устаревает: её контент в GPU hot loop никто не читает.
        const float lr = neighborList_.listRadius();
        gpu_->rebuildNeighborListOnGpu(grid.size.x, grid.size.y, grid.size.z, grid.cellSize,
                                       static_cast<uint32_t>(grid.countCells), lr * lr);
    };

    if (auto disp = gpu_->tryConsumeMaxDisplacementSqr(); disp.has_value()) {
        // Готовый результат забран без столла.
        if (*disp > thresholdSqr) {
            rebuildNeighborList();
        }
    }
    else if (gpu_->dispCheckPending() && gpu_->dispCheckAgeSteps() >= kDispReadbackMaxLagSteps) {
        // Backstop: дольше лимита нельзя ждать — дождаться сейчас, до шага со старым NL.
        if (gpu_->finishMaxDisplacementSqrBlocking() > thresholdSqr) {
            rebuildNeighborList();
        }
    }
    if (!gpu_->dispCheckPending() && stepsSinceDispCheck_ >= kDispCheckCadence) {
        // Нет pending и каденция истекла — запустить новую async-редукцию.
        stepsSinceDispCheck_ = 0;
        gpu_->beginMaxDisplacementSqrAsync();
    }

    gpu_->step(state_.Dt, state_.integrator.accelDamping());
    ++stepsSinceDispCheck_;
    cpuPositionsDirty_ = true; // VRAM новее CPU-копии
    state_.metricsCacheValid_ = false;
    ++state_.sim_step;
    state_.sim_time_ns += state_.Dt * Units::kTimeUnitToNs;
}

void World::uploadSceneToGpu() {
    // Любой pending async disp-check считался против старого refPos — после
    // перезаливки он устареет, а его буфер сейчас mid-map. Безопасно снести.
    gpu_->discardPendingDisplacementCheck();

    // Если GPU ушёл вперёд (CPU-копия устарела), сперва слить его прогресс в CPU,
    // иначе полный upload откатил бы существующие атомы к последнему CPU-снимку.
    // Усечённая выгрузка (downloadToCpu clamp) трогает только старый префикс —
    // свежедобавленный хвост остаётся из CPU. clear() заранее сбрасывает dirty,
    // поэтому сброшенная сцена тут не «воскресает». In-place правки (термо-тул,
    // resize-сдвиг) синкаются ДО правки сами, так что здесь их dirty уже снят.
    if (cpuPositionsDirty_) {
        gpu_->downloadToCpu(atomStorage_, /*withVelocities=*/true);
        cpuPositionsDirty_ = false;
    }

    // NL в Full (требование резидентного LJ-пути), свежий build, полная заливка
    // позиций/скоростей/типов/NL в VRAM. ensureCapacity растит буфера под текущий
    // размер сцены — поэтому путь годится и для входа в GPU-режим, и для пере-синка
    // после правки сцены (add/remove/load).
    //
    // ЗАВИСИМОСТЬ ОТ NeighborList::setMode(Full): резидентный LJ-путь требует Full-NL
    // (каждая пара дважды, force loop пишет только в свой forceX — нет race). Этот
    // вызов восстановлен из нашего форка; см. разрешение конфликта NeighborList.
    neighborList_.setMode(NeighborListMode::Full);
    neighborList_.rebuildPipeline(atomStorage_, *this, static_cast<int>(state_.sim_step));

    // gravity берём из World — резидентный wall-kernel считает её как постоянную СИЛУ.
    // Рантайм-смена gravity бампит cpuSceneVersion_ (World::setGravity) → этот
    // re-upload несёт новое значение.
    gpu_->uploadFromCpu(atomStorage_, neighborList_, state_.forceField_.ljForceField(),
                        static_cast<float>(size.x), static_cast<float>(size.y), static_cast<float>(size.z),
                        static_cast<float>(gravity.x), static_cast<float>(gravity.y), static_cast<float>(gravity.z),
                        ljEnabled, coulombEnabled);
    // Bond-adjacency (CSR) заливаем рядом с позициями/NL: статичная топология
    // существующих связей получает Morse-силы на GPU (2.2a). Сцена без связей →
    // пустой CSR (силы 0, LJ-only паритет цел).
    gpu_->uploadBonds(bonds_, atomStorage_);
    cpuPositionsDirty_ = false;
    stepsSinceDispCheck_ = 0;
    gpuUploadedSceneVersion_ = cpuSceneVersion_; // VRAM теперь соответствует CPU-сцене
}

void World::setGpuMode(bool enable) {
    if (enable == static_cast<bool>(gpu_)) {
        return; // уже в нужном режиме
    }

    if (enable) {
        // CPU -> GPU: создаём резидентность и заливаем текущую сцену.
        gpu_ = std::make_unique<GpuResidentPhysics>();
        uploadSceneToGpu();
    }
    else {
        // GPU -> CPU: вернуть актуальное состояние в AtomStorage, снести GPU,
        // пометить NL на перестройку (позиции изменились).
        gpu_->discardPendingDisplacementCheck(); // буфер не должен остаться mid-map при сносе
        gpu_->downloadToCpu(atomStorage_, /*withVelocities=*/true);
        gpu_.reset();
        cpuPositionsDirty_ = false;
        neighborList_.clear();
    }
    state_.metricsCacheValid_ = false;
}

void World::syncFromGpuIfNeeded() {
    if (gpu_ && cpuPositionsDirty_) {
        gpu_->downloadToCpu(atomStorage_, /*withVelocities=*/true);
        cpuPositionsDirty_ = false;
        state_.metricsCacheValid_ = false;
    }
}

void World::syncGpuBeforeEdit() {
    if (gpu_ && cpuPositionsDirty_) {
        gpu_->downloadToCpu(atomStorage_, /*withVelocities=*/true);
        cpuPositionsDirty_ = false;
    }
}

void World::refreshDiagnosticsGrid() {
    if (!gpu_) {
        return; // CPU-мир: грид перестраивается физическим шагом, уже свежий.
    }
    // Только биннинг грида из УЖЕ синканных CPU-позиций (precondition: вызывающий
    // сделал syncFromGpuIfNeeded в начале кадра). НЕ качаем VRAM повторно и НЕ
    // трогаем CPU NeighborList — viz/overlay/stats читают именно грид, а NL-readback
    // на каждый кадр был бы дорог (это диагностика, не hot loop).
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
}

World::GpuDispCounts World::gpuDispCounts() const noexcept {
    if (!gpu_) {
        return {};
    }
    return {gpu_->dispBeginCount(), gpu_->dispConsumeCount(), gpu_->dispBackstopCount()};
}
