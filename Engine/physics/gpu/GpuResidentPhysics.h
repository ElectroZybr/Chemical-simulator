#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <webgpu/webgpu-raii.hpp>
#include <webgpu/webgpu.hpp>

#include "Engine/physics/Bond.h" // Bond::List (nested typedef — forward-decl недостаточно)

class AtomStorage;
class NeighborList;
class LJForceField;
class GpuNeighborListBuilder;

// Резидентная GPU-физика: позиции/скорости/силы живут в VRAM между шагами,
// CPU их не качает в hot loop. Это и есть «физика на GPU» (в отличие от
// GpuPairForceCompute, который оффлоадит одну операцию с readback каждый раз).
//
// Что считается на GPU: LJ + soft-wall + gravity + Morse- и угловые силы статичных
// связей. Ограничения GPU-режима: Coulomb выключен; bonds — силы (Morse + angle)
// считаются по СТАТИЧНОЙ топологии (формация/разрыв заморожены, пока активен
// GPU-режим); NeighborList в режиме Full (каждая пара дважды, force loop пишет
// только в свой forceX — нет race). Эти ограничения — следствие резидентности:
// если бы силы читал CPU, пришлось бы качать позиции каждый шаг.
// (Soft-wall/gravity — per-atom-силы без neighbor-чтения, поэтому легли на GPU
// без CPU round-trip: зеркалят WallForceField, паритет проверяет BM_GpuWallGravityParity.
// Morse — per-atom gather по bond-CSR, зеркалит Bond::forceBond; angle — двух-ролевой
// per-atom gather по той же CSR, зеркалит Bond::angleForce; паритет — BM_GpuBondParity.)
//
// Шаг повторяет CPU velocity Verlet (VerletScheme + StepOps::confineToBox):
//   predict -> confine -> swap(pf<->f, parity) -> zero(f, total) -> wall+gravity -> LJ -> bond_morse -> bond_angle -> correct
// swap реализован ping-pong'ом двух force-буферов через parity-бит и две
// пред-собранные bind-группы (не копирование).
class GpuResidentPhysics {
public:
    GpuResidentPhysics();
    ~GpuResidentPhysics();

    GpuResidentPhysics(const GpuResidentPhysics&) = delete;
    GpuResidentPhysics& operator=(const GpuResidentPhysics&) = delete;

    // Заливает полное состояние атомов + NL из CPU в VRAM. Вызывается при входе
    // в GPU-режим и после любой структурной правки (add/remove atom, NL rebuild).
    // gravity — постоянная сила wall-ядра (world.getGravity()); заливается в
    // wallUniform_. Полная перезаливка несёт текущую gravity, поэтому рантайм-смена
    // gravity (через cpuSceneVersion-бамп в Simulation::setGravity) подхватывается
    // ближайшим re-upload'ом.
    // ljEnabled — world.isLJEnabled(): если false, step() ПРОПУСКАЕТ диспатч
    // compute_lj (как CPU ForceField::compute чекает isLJEnabled, ForceField.cpp:147-150).
    // Раньше GPU-шаг диспатчил LJ безусловно — setLJEnabled(false) молча игнорился
    // (тихая дивергенция). Полная перезаливка несёт текущий флаг, поэтому рантайм-
    // смена (через cpuSceneVersion-бамп) подхватывается ближайшим re-upload'ом.
    void uploadFromCpu(const AtomStorage& atoms, const NeighborList& neighborList, const LJForceField& ljForceField,
                       float worldSizeX, float worldSizeY, float worldSizeZ, float gravityX, float gravityY, float gravityZ,
                       bool ljEnabled);

    // Только NL (offsets+neighbors) + refPos для displacement-проверки. Вызывается
    // после CPU NeighborList::build, без перезаливки позиций/скоростей.
    void uploadNeighborList(const NeighborList& neighborList);

    // Заливает резидентную bond-adjacency (CSR) из CPU-списка связей. Каждая связь
    // (a,b) → ДВА directed edge (a→b и b→a) с per-edge Morse-параметрами из
    // bond.params (Bond.h:40). Зеркалит порядок CPU-построения adjacency
    // (BondForceField.cpp:129-134): для bond (a,b) сосед b добавляется атому a,
    // сосед a — атому b, в порядке обхода списка bonds. Буфера растут в
    // ensureCapacity под 2*bondCount рёбер ДО writeBuffer (fail-closed). Вызывается
    // из Simulation::uploadSceneToGpu рядом с uploadFromCpu. Статичная топология:
    // связи неизменны, пока активен GPU-режим (формация/разрыв заморожены).
    void uploadBonds(const Bond::List& bonds, const AtomStorage& atoms);

    // Шаг 2c: пересобирает Full NeighborList ЦЕЛИКОМ на GPU из резидентных
    // positions_ и оставляет результат в резидентных nlOffsets_/nlNeighbors_, что
    // читает LJ-ядро. БЕЗ CPU rebuild и БЕЗ скачивания позиций. Внутренний
    // GpuNeighborListBuilder строит NL в свои shadow-буфера (GPU-fed позиции через
    // GPU->GPU copy), затем GPU->GPU копируем offsets/neighbors в резидентные.
    // Параметры сетки приходят от вызывающего (как worldSize в uploadFromCpu) и
    // обязаны совпадать с CPU SpatialGrid: sizeX/Y/Z, cellSize, cellCount, и
    // listRadiusSqr = r_list^2 (cutoff+skin)^2 (== NeighborList::listRadiusSqr_).
    //
    // Overflow fail-closed: total соседей известен после count+scan (скалярный
    // readback в builder). Если он превышает резидентную ёмкость nlNeighbors_ —
    // РАСТИМ резидентный буфер (rebuildBindGroups перепривязывает LJ-группу) ДО
    // GPU->GPU copy; частичный/усечённый NL в LJ-ядро не попадает никогда.
    // refPos_ обновляется (база displacement-проверки), как в uploadNeighborList.
    //
    // ВНИМАНИЕ (2c): метод НЕ вызывается из step()/updateStateGpu — hot loop пока
    // на CPU rebuild (swap — задача 2d). Добавлен и верифицируется bench-гейтом.
    void rebuildNeighborListOnGpu(uint32_t gridSizeX, uint32_t gridSizeY, uint32_t gridSizeZ, float cellSize, uint32_t cellCount,
                                  float listRadiusSqr);

    // Телеметрия 2c: сколько раз резидентный nlNeighbors_ пришлось вырастить под
    // GPU-NL total (overflow относительно прежней ёмкости). Для bench/диагностики.
    [[nodiscard]] uint64_t nlCapacityGrows() const noexcept { return nlCapacityGrows_; }

    // Телеметрия 2e: сколько GPU-перестроек NL случилось (по разу на каждый
    // rebuildNeighborListOnGpu). GPU-аналог CPU NeighborList::stats().rebuildCount()
    // — после 2d hot loop перестраивает NL на GPU, а не на CPU, поэтому CPU-счётчик
    // в GPU-режиме всегда 0 и вводит в заблуждение. Этот счётчик его замещает.
    [[nodiscard]] uint64_t nlRebuildCount() const noexcept { return nlRebuilds_; }

    // Блокирующий readback РЕЗИДЕНТНЫХ NL-буферов (bench/диагностика — дренит
    // очередь, не для hot loop). Доказывает, что именно резидентные nlOffsets_/
    // nlNeighbors_ (которые читает LJ-ядро) держат корректный NL после GPU-rebuild.
    //   readbackNlOffsets():    totalCount()+1 элементов (CSR; [totalCount]=total).
    //   readbackNlNeighbors(n): ровно n элементов (n = offsets[totalCount]).
    [[nodiscard]] std::vector<uint32_t> readbackNlOffsets() const;
    [[nodiscard]] std::vector<uint32_t> readbackNlNeighbors(uint32_t total) const;

    // Блокирующий readback РЕЗИДЕНТНЫХ bond-CSR буферов (bench/диагностика — как
    // readbackNl*). Доказывает, что bond-adjacency реально доставлена в VRAM
    // (а не молча пуста → 0 bond-сил → ложный pass гейта, как устаревшая gravity).
    //   readbackBondOffsets():    totalCount()+1 элементов (CSR; [totalCount]=2*bondCount).
    //   readbackBondNeighbors(n): ровно n directed-рёбер (n = bondOffsets[totalCount]).
    [[nodiscard]] std::vector<uint32_t> readbackBondOffsets() const;
    [[nodiscard]] std::vector<uint32_t> readbackBondNeighbors(uint32_t total) const;

    // Один резидентный шаг (dt, accelDamping как у CPU Integrator). Ничего не
    // качает CPU<->GPU. Допускает батчинг (несколько step() подряд до sync).
    void step(float dt, float accelDamping);

    // Скачивает позиции (и опц. скорости) обратно в CPU AtomStorage. Нужно для
    // NL rebuild, рендера, метрик — редкие sync-точки, не каждый шаг.
    void downloadToCpu(AtomStorage& atoms, bool withVelocities = true);

    // Телеметрия Инкремента B (zero-copy): сколько РЕАЛЬНЫХ GPU->CPU скачиваний
    // случилось (по разу на каждый непустой downloadToCpu). Доказывает выигрыш:
    // в «чистом» GPU-режиме (атомы only, atom-color, без bonds/grid/панелей/спид-
    // цвета-авто) условный per-frame sync НЕ зовёт downloadToCpu, поэтому счётчик
    // за N рендер-кадров не растёт; при активном CPU-потребителе позиций — растёт.
    [[nodiscard]] uint64_t downloadCount() const noexcept { return downloadCount_; }

    // Возвращает максимум |pos - refPos|^2 по mobile-атомам через GPU-редукцию
    // (для решения нужен ли NL rebuild). Читает 4 байта, но БЛОКИРУЕТ (dev.poll)
    // — дренит GPU-очередь. Оставлен для совместимости/backstop.
    float maxDisplacementSqr();

    // Асинхронный disp-check без блокирующего poll'а (резидентный пайплайн не
    // сериализуется). begin запускает редукцию + неблокирующий mapAsync;
    // tryConsume неблокирующе прокручивает callback и отдаёт результат когда
    // готов; finishBlocking форсирует завершение (hard backstop по возрасту);
    // discard безопасно сносит pending (после reupload refPos устарел).
    // Single-in-flight: begin — no-op пока предыдущий не завершён.
    void beginMaxDisplacementSqrAsync();
    [[nodiscard]] std::optional<float> tryConsumeMaxDisplacementSqr();
    // Дождаться pending-результата блокирующе. PRECONDITION: вызывать только когда
    // dispCheckPending() — иначе dispMap_.done может быть stale-true от прошлого
    // и read пойдёт по unmapped-буферу. Все вызовы охраняются dispCheckPending().
    float finishMaxDisplacementSqrBlocking();
    void discardPendingDisplacementCheck();
    [[nodiscard]] bool dispCheckPending() const noexcept { return dispCheckPending_; }
    [[nodiscard]] int dispCheckAgeSteps() const noexcept { return dispCheckAgeSteps_; }
    // Телеметрия (бенч/диагностика): сколько disp-check'ов забрано async без столла
    // против сколько ушло в блокирующий backstop. Доля backstop≈1 => бенч глушит async.
    [[nodiscard]] uint64_t dispConsumeCount() const noexcept { return dispConsumeCount_; }
    [[nodiscard]] uint64_t dispBackstopCount() const noexcept { return dispBackstopCount_; }
    [[nodiscard]] uint64_t dispBeginCount() const noexcept { return dispBeginCount_; }

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    // Число атомов, под которое сейчас залиты резидентные буфера. Сравнивается
    // с CPU AtomStorage::size() для детекта правки сцены при включённом GPU.
    [[nodiscard]] uint32_t totalCount() const noexcept { return totalCount_; }

    // --- Read-only render-bind seam (zero-copy рендер в GPU-режиме) ---
    // Отдают СЫРОЙ non-owning handle резидентных pos/vel (НЕ raii — владение
    // остаётся у физики; рендер биндит как ReadOnlyStorage и НЕ пишет). Формат —
    // array<vec4<f32>> (16 байт/атом, как ждёт шейдер atom2d/atom3d binding 1/2),
    // размер буфера >= totalCount*16. Зеркалит GpuNeighborListBuilder::nlOffsetsBuffer().
    // ВАЖНО: handle протухает при росте сцены (ensureCapacity пересоздаёт буфера),
    // поэтому рендер обязан пере-биндить, когда renderBufferGeneration() изменилась.
    // Биндить/рисовать можно ровно renderBoundCount() атомов.
    [[nodiscard]] wgpu::Buffer positionsBuffer() const noexcept { return *positions_; }
    [[nodiscard]] wgpu::Buffer velocitiesBuffer() const noexcept { return *velocities_; }
    // Сколько атомов рендеру разрешено биндить/рисовать из резидентных буферов
    // (== totalCount_ — число залитых в VRAM атомов). Отдельное имя документирует
    // render-контракт «сколько биндить», который семантически совпадает с totalCount.
    [[nodiscard]] uint32_t renderBoundCount() const noexcept { return totalCount_; }
    // Счётчик пересозданий резидентных pos/vel-буферов. Растёт ровно когда
    // ensureCapacity пересоздаёт positions_/velocities_ (рост atom-ёмкости) — сигнал
    // рендеру «handle протух, пере-биндить». НЕ тикает на росте NL-буферов.
    [[nodiscard]] uint64_t renderBufferGeneration() const noexcept { return bufferGeneration_; }

private:
    void ensureInitialized();
    void ensureCapacity(size_t totalCount, size_t mobileCount, size_t neighborCount);
    void rebuildBindGroups();
    // Запуск GPU-редукции смещения + mapAsync (без блокировки). Общий для async-
    // begin и блокирующего maxDisplacementSqr.
    void submitDisplacementReductionAndMap();
    // Чтение готового результата dispReadback_ (mapped), unmap, снятие pending.
    // Вызывать только когда dispMap_.done. Возвращает max|disp|^2.
    float readDisplacementResultAndClear();

    bool initialized_ = false;
    bool ljTableUploaded_ = false;

    uint32_t mobileCount_ = 0;
    uint32_t totalCount_ = 0;
    // Счётчик пересозданий резидентных positions_/velocities_ (render-bind seam).
    // Инкремент в ensureCapacity при росте atom-ёмкости (handle протухает). Рендер
    // сравнивает его, чтобы пере-биндить чужой буфер. См. renderBufferGeneration().
    uint64_t bufferGeneration_ = 0;
    int parity_ = 0; // какой из forces_[2] сейчас «current»
    // LJ on/off (world.isLJEnabled()): step() пропускает диспатч compute_lj когда
    // false. Доставляется на uploadFromCpu (полная перезаливка несёт текущий флаг).
    // По умолчанию true — дефолтное поведение (LJ включён) не меняется.
    bool ljEnabled_ = true;
    // Число directed-рёбер в bond-CSR (= 2*bondCount), залитых в bondNeighbors_.
    // 0 при сцене без связей → bond-kernel при пустых offsets прибавляет ровно 0.
    uint32_t bondNeighborCount_ = 0;

    float cutoffSqr_ = 0.0f;
    float worldMax_[3] = {0, 0, 0};
    // Gravity (постоянная СИЛА wall-ядра, world.getGravity()) — обновляется на
    // каждом uploadFromCpu (полная перезаливка несёт текущую gravity). k/border —
    // CPU-инварианты модели (== WallForceField.cpp:28-29), лежат в wallUniform_
    // рядом с gravity для читаемости («эти числа = CPU k/border»).
    float gravity_[3] = {0, 0, 0};

    // Pipelines
    wgpu::raii::ComputePipeline ljPipeline_;
    wgpu::raii::ComputePipeline wallPipeline_;
    wgpu::raii::ComputePipeline bondMorsePipeline_; // 2.2a: Morse-силы статичных связей
    wgpu::raii::ComputePipeline bondAnglePipeline_; // 2.2b: угловые силы статичных связей
    wgpu::raii::ComputePipeline predictPipeline_;
    wgpu::raii::ComputePipeline confinePipeline_;
    wgpu::raii::ComputePipeline zeroPipeline_;
    wgpu::raii::ComputePipeline correctPipeline_;
    wgpu::raii::ComputePipeline displacementPipeline_;

    wgpu::raii::BindGroupLayout ljLayout_;
    wgpu::raii::BindGroupLayout wallLayout_;
    wgpu::raii::BindGroupLayout bondMorseLayout_; // 2.2a
    wgpu::raii::BindGroupLayout bondAngleLayout_; // 2.2b (5 bindings: без bondParams)
    wgpu::raii::BindGroupLayout intLayout_;
    wgpu::raii::BindGroupLayout dispLayout_;

    // Резидентные буфера
    wgpu::raii::Buffer positions_;
    wgpu::raii::Buffer velocities_;
    wgpu::raii::Buffer forces_[2]; // ping-pong: current / prev
    wgpu::raii::Buffer invMass_;
    wgpu::raii::Buffer types_;
    wgpu::raii::Buffer nlOffsets_;
    wgpu::raii::Buffer nlNeighbors_;
    wgpu::raii::Buffer ljPairs_;
    // Bond-adjacency CSR (2.2a, зеркалит nlOffsets_/nlNeighbors_): bondOffsets_
    // (totalCount+1), bondNeighbors_ (2*bondCount directed-рёбер), bondParams_
    // (vec4 r0/De/a/_ на ребро, параллельно bondNeighbors_). Заливаются в uploadBonds.
    wgpu::raii::Buffer bondOffsets_;
    wgpu::raii::Buffer bondNeighbors_;
    wgpu::raii::Buffer bondParams_;
    wgpu::raii::Buffer refPos_;          // позиции на момент последнего NL build
    wgpu::raii::Buffer dispFlag_;        // 1×u32, atomicMax bitcast displacement^2
    wgpu::raii::Buffer dispReadback_;    // MapRead для dispFlag_
    wgpu::raii::Buffer posReadback_;     // MapRead для downloadToCpu
    wgpu::raii::Buffer velReadback_;

    wgpu::raii::Buffer ljUniform_;
    wgpu::raii::Buffer wallUniform_;
    wgpu::raii::Buffer bondUniform_; // 2.2a: {totalCount, thetaZero, kAngle, pad}
    wgpu::raii::Buffer intUniform_;
    wgpu::raii::Buffer dispUniform_;

    // Bind-группы для parity 0 и 1 (current/prev меняются местами).
    wgpu::raii::BindGroup ljBindGroup_[2];
    wgpu::raii::BindGroup wallBindGroup_[2]; // wall+gravity: forces -> forces_[p]
    wgpu::raii::BindGroup bondMorseBindGroup_[2]; // 2.2a: forces -> forces_[p]
    wgpu::raii::BindGroup bondAngleBindGroup_[2]; // 2.2b: forces -> forces_[p]
    wgpu::raii::BindGroup intBindGroup_[2];
    wgpu::raii::BindGroup zeroBindGroup_[2];
    wgpu::raii::BindGroup dispBindGroup_;

    size_t atomCapacity_ = 0;

    // Состояние async disp-check (single-in-flight). Callback (mapAsync,
    // AllowSpontaneous) по контракту webgpu.h может сработать на ПРОИЗВОЛЬНОМ
    // потоке — поэтому флаги атомарные (callback store, main-thread load); сам
    // callback больше ничего не делает (без re-entrant webgpu-вызовов). dispMap_
    // — член (стабильный адрес, переживает begin); read/unmap на main-thread.
    struct DispMapState {
        std::atomic<bool> done{false};
        std::atomic<bool> ok{false};
    };
    DispMapState dispMap_{};
    bool dispCheckPending_ = false; // readback запущен, ещё не consume/finish
    int dispCheckAgeSteps_ = 0;     // physics-шагов с момента begin (для backstop)

    // Телеметрия async disp-check (для бенч-матрицы: доля backstop vs async-consume).
    uint64_t dispBeginCount_ = 0;
    uint64_t dispConsumeCount_ = 0;   // async-результат забран без столла
    uint64_t dispBackstopCount_ = 0;  // пришлось блокирующе дождаться (finishBlocking)
    uint64_t dispDiscardCount_ = 0;
    size_t nlOffsetsCapacity_ = 0;
    size_t nlNeighborsCapacity_ = 0;
    // Ёмкости bond-CSR буферов (2.2a). bondOffsets_ растёт под totalCount+1 (тот же
    // размер, что nlOffsets_, но отдельный буфер); bondNeighbors_/bondParams_ — под
    // 2*bondCount directed-рёбер. Растут в ensureCapacity, fail-closed до writeBuffer.
    size_t bondOffsetsCapacity_ = 0;
    size_t bondNeighborsCapacity_ = 0;

    // Шаг 2c: внутренний GPU NL builder (cell-list+scan+Full NL целиком на GPU).
    // Lazy-инициализируется при первом rebuildNeighborListOnGpu (резидентные
    // инстансы, которые им не пользуются, не платят за его буфера). unique_ptr,
    // т.к. builder некопируем/неперемещаем (владеет raii-буферами).
    std::unique_ptr<GpuNeighborListBuilder> nlBuilder_;
    uint64_t nlCapacityGrows_ = 0; // сколько раз резидентный nlNeighbors_ рос под GPU-NL total
    uint64_t nlRebuilds_ = 0;      // 2e: сколько GPU-перестроек NL (по разу на rebuildNeighborListOnGpu)
    uint64_t downloadCount_ = 0;   // B: сколько реальных GPU->CPU downloadToCpu (perf-гейт zero-copy)
};
