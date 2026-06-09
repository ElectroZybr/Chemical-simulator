#include "NeighborList.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/World.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/AtomStorage.h"
#include "Engine/restrict.h"

#ifdef LATTICELAB_USE_TBB
#include <tbb/blocked_range.h>
#include <tbb/global_control.h>
#include <tbb/parallel_for.h>
#include <tbb/task_arena.h>
#endif

namespace {
// Порог распараллеливания построения NL: ниже — серийно (накладные TBB не окупаются).
// Значение совпадает с ForceField::kParallelMobileThreshold, но НЕЗАВИСИМ — тюнится
// отдельно (стоимость построения NL не равна стоимости force-loop).
constexpr uint32_t kParallelBuildThreshold = 5000;

#ifdef LATTICELAB_USE_TBB
// Минимальная РЕАЛЬНАЯ параллельность, при которой параллельная форма данных окупается.
// Full NL удваивает парную работу, 3-проходный build добавляет проходы — на 1-2 ядрах это
// чистый штраф (был single-thread регресс). Поэтому выбор Full/parallel гейтится не только
// числом атомов, но и фактическим параллелизмом: 1-3 ядра идут Half + serial (как чистый
// upstream), >= 4 — параллельная форма с выигрышем.
constexpr size_t kMinParallelConcurrency = 4;

// Фактический доступный параллелизм TBB. hardware_concurrency() тут НЕ годится: он не видит
// tbb::global_control(max_allowed_parallelism) (например принудительный 1 поток) и ограничения
// арены. Берём минимум глобального лимита потоков и ёмкости текущей арены.
size_t effectiveConcurrency() {
    const size_t globalLimit =
        tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism);
    const int arena = tbb::this_task_arena::max_concurrency();
    const size_t arenaLimit = arena > 0 ? static_cast<size_t>(arena) : 1;
    return std::min(globalLimit, arenaLimit);
}
#endif
} // namespace

void NeighborList::setCutoff(float cutoff) {
    cutoff_ = cutoff;
    listRadius_ = cutoff_ + skin_;
    listRadiusSqr_ = listRadius_ * listRadius_;
    valid_ = false;
}

void NeighborList::setSkin(float skin) {
    skin_ = skin;
    listRadius_ = cutoff_ + skin_;
    listRadiusSqr_ = listRadius_ * listRadius_;
    valid_ = false;
}

void NeighborList::setParams(float cutoff, float skin) {
    cutoff_ = cutoff;
    skin_ = skin;
    listRadius_ = cutoff_ + skin_;
    listRadiusSqr_ = listRadius_ * listRadius_;
    valid_ = false;
}

void NeighborList::setMode(NeighborListMode mode) {
    autoMode_ = false;
    if (mode_ != mode) {
        mode_ = mode;
        valid_ = false;
    }
}

void NeighborList::setAutoMode(size_t threshold) {
    if (!autoMode_ || autoThreshold_ != threshold) {
        autoMode_ = true;
        autoThreshold_ = threshold;
        valid_ = false;
    }
}

void NeighborList::clear() {
    neighbors_.clear();
    offsets_.clear();
    refPosX_.clear();
    refPosY_.clear();
    refPosZ_.clear();
    neighbors_.shrink_to_fit();
    offsets_.shrink_to_fit();
    refPosX_.shrink_to_fit();
    refPosY_.shrink_to_fit();
    refPosZ_.shrink_to_fit();
    valid_ = false;
    resetStats();
}

void NeighborList::rebuildPipeline(const AtomStorage& atoms, World& world, int simStep) {
    // перестройка пространственной сетки
    world.getGrid().rebuild(atoms.xDataSpan(), atoms.yDataSpan(), atoms.zDataSpan());
    // перестройка списка соседей (контракт cellSize >= listRadius проверяется в build)
    build(atoms, world);
    // обновление метрик
    const float rebuildTimeMs = static_cast<float>(Profiler::instance().lastMs("NeighborList::build"));
    stats_.recordRebuild(simStep, rebuildTimeMs);
}

void NeighborList::build(const AtomStorage& atoms, World& box) {
    PROFILE_SCOPE("NeighborList::build");

    // 27-cell стенсил покрывает только если cellSize >= listRadius. Иначе пары на
    // расстоянии (cellSize, listRadius] окажутся за пределами обхода и тихо выпадут
    // из NL — force loop их не учтёт. Это контракт, а не warning. Проверяем здесь,
    // на публичном входе build(), а не только в rebuildPipeline — иначе прямой
    // вызов build() (тесты, бенчи, будущий код) обходит контракт.
    const float cellSize = box.getGrid().cellSize;
    if (cellSize + 1e-6f < listRadius_) {
        throw std::invalid_argument(
            "NeighborList::build: cellSize must be >= listRadius (cutoff + skin); "
            "27-cell stencil cannot cover the radius otherwise");
    }

    // Auto-mode: режим выбирается по mobileCount И по реальному параллелизму на каждом rebuild.
    // Half дёшев на малых сценах (1x работа, нет 2x памяти NL) И на малоядерных машинах: Full
    // окупается, только когда force loop реально параллелится. Без гейта по ядрам на 1-2 ядрах
    // Full давал 2x лишней работы без выигрыша — это и был single-thread регресс.
    if (autoMode_) {
        bool useFull = atoms.mobileCount() >= autoThreshold_;
#ifdef LATTICELAB_USE_TBB
        useFull = useFull && effectiveConcurrency() >= kMinParallelConcurrency;
#else
        useFull = false; // без TBB параллелить нечем — Full только тратит работу
#endif
        mode_ = useFull ? NeighborListMode::Full : NeighborListMode::Half;
    }

    const SpatialGrid& grid = box.getGrid();
    const uint32_t atomCount = static_cast<uint32_t>(atoms.size());
    const float* RESTRICT x = atoms.xData();
    const float* RESTRICT y = atoms.yData();
    const float* RESTRICT z = atoms.zData();

    reserveListBuffers(atoms);

    // Построение списка соседей.
    //  • Большие сцены (TBB): count → scan смещений → write. Фаза 1 параллельно считает
    //    соседей каждого атома (offsets_[i+1]), фаза 2 — exclusive prefix-sum, фаза 3
    //    параллельно пишет соседей в СВОЙ срез neighbors_[offsets_[i]..]. Атомы независимы
    //    (грид/позиции read-only, срезы не пересекаются) → без гонок, результат ПОБИТОВО
    //    как серийный (тот же порядок).
    //  • Малые сцены / без TBB: один проход emplace_back (накладные параллелизма не окупаются).
#ifdef LATTICELAB_USE_TBB
    if (atomCount >= kParallelBuildThreshold && effectiveConcurrency() >= kMinParallelConcurrency) {
        tbb::parallel_for(tbb::blocked_range<uint32_t>(0, atomCount), [&](const tbb::blocked_range<uint32_t>& r) {
            for (uint32_t i = r.begin(); i != r.end(); ++i) {
                offsets_[i + 1] = countAtomNeighbors(grid, x, y, z, i, x[i], y[i], z[i]);
            }
        });

        // exclusive prefix-sum в uint64 со страховкой: суммарное число пар не должно
        // превысить uint32-ёмкость CSR (offsets_/neighbors_ — uint32), иначе фаза 3 писала
        // бы в обёрнутые/выходящие за буфер срезы. Fail-closed: бросаем ДО resize/записи
        // (серийный путь лишь молча обрезал offsets — тоже неверно, но без OOB).
        uint64_t running = 0;
        offsets_[0] = 0;
        for (uint32_t i = 0; i < atomCount; ++i) {
            running += offsets_[i + 1]; // держит счётчик соседей атома i (из фазы 1)
            if (running > std::numeric_limits<uint32_t>::max()) {
                throw std::overflow_error("NeighborList::build: суммарное число пар превышает uint32-ёмкость CSR");
            }
            offsets_[i + 1] = static_cast<uint32_t>(running);
        }

        neighbors_.resize(offsets_[atomCount]);
        tbb::parallel_for(tbb::blocked_range<uint32_t>(0, atomCount), [&](const tbb::blocked_range<uint32_t>& r) {
            for (uint32_t i = r.begin(); i != r.end(); ++i) {
                writeAtomNeighborsAt(grid, x, y, z, i, x[i], y[i], z[i], neighbors_.data() + offsets_[i]);
            }
        });
    } else
#endif
    {
        offsets_[0] = 0;
        for (uint32_t i = 0; i < atomCount; ++i) {
            writeAtomNeighbors(grid, x, y, z, i, x[i], y[i], z[i], neighbors_);
            offsets_[i + 1] = neighbors_.size();
        }
    }

    std::copy(x, x + atoms.mobileCount(), refPosX_.data());
    std::copy(y, y + atoms.mobileCount(), refPosY_.data());
    std::copy(z, z + atoms.mobileCount(), refPosZ_.data());

    valid_ = true;
}

bool NeighborList::needsRebuild(const AtomStorage& atoms) const {
    const size_t n = atoms.mobileCount();

    if (!valid_ || n != refPosX_.size()) {
        return true;
    }

    const float maxDisp = (0.5f * skin_);
    const float maxDispSqr = maxDisp * maxDisp;

    const float* RESTRICT x = atoms.xData();
    const float* RESTRICT y = atoms.yData();
    const float* RESTRICT z = atoms.zData();

    const float* RESTRICT refX = refPosX_.data();
    const float* RESTRICT refY = refPosY_.data();
    const float* RESTRICT refZ = refPosZ_.data();

    int rebuild = false;
#pragma GCC ivdep
    for (uint32_t i = 0; i < n; ++i) {
        const float dx = x[i] - refX[i];
        const float dy = y[i] - refY[i];
        const float dz = z[i] - refZ[i];
        rebuild |= ((dx * dx + dy * dy + dz * dz) > maxDispSqr);
    }

    return rebuild;
}

uint32_t NeighborList::atomCount() const {
    if (offsets_.empty()) {
        return 0;
    }
    return offsets_.size() - 1;
}

uint32_t NeighborList::pairStorageSize() const {
    return std::min(neighbors_.size(), static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
}

uint32_t NeighborList::memoryBytes() const {
    return neighbors_.capacity() * sizeof(uint32_t) + offsets_.capacity() * sizeof(uint32_t) + refPosX_.capacity() * sizeof(float) +
           refPosY_.capacity() * sizeof(float) + refPosZ_.capacity() * sizeof(float);
}

void NeighborList::resetStats() { stats_.reset(); }

void NeighborList::reserveListBuffers(const AtomStorage& atoms) {
    const size_t prevCapacity = neighbors_.capacity();
    neighbors_.clear();
    offsets_.resize(atoms.size() + 1);
    refPosX_.resize(atoms.mobileCount());
    refPosY_.resize(atoms.mobileCount());
    refPosZ_.resize(atoms.mobileCount());

    // первый build — fallback, потом реальный размер из прошлого раза
    if (prevCapacity > 0) {
        neighbors_.reserve(prevCapacity);
    }
    else {
        neighbors_.reserve(atoms.size() * 64);
    }
}
