#include "World.h"

#include "Engine/metrics/EnergyMetrics.h"
#include "Engine/physics/Integrator.h"

World::World(glm::vec3 size, glm::vec3 renderOffset) : size(size), renderOffset(renderOffset), grid(size), vectorField_(glm::ivec3(size), 0) {
    atomStorage_.reserve(250000);
    neighborList_.setParams(5.f, 1.f);
}

void World::clear() {
    clearAtoms();
    clearBonds();
    neighborList_.clear();
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
    invalidateMetrics();
    invalidateVectorField();
}

void World::reset() {
    clear();
    clearMetadata();
    resetRuntimeState();
}

void World::resizeBox(const glm::vec3& newSize, float cellSize) {
    setWorldSize(newSize);
    setGridCellSize(cellSize);
    finalizeAtomBatch();
}

void World::addAtom(const glm::vec3& start_coords, const glm::vec3& start_speed, AtomData::Type type, bool fixed) {
    atomStorage_.addAtom(start_coords, start_speed, type, fixed);
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
    invalidateMetrics();
    invalidateVectorField();
}

void World::addBond(size_t aIndex, size_t bIndex) { Bond::CreateBond(bonds_, aIndex, bIndex, atomStorage_); }

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
    invalidateVectorField();
}

void World::finalizeAtomBatch() {
    grid.rebuild(atomStorage_.xDataSpan(), atomStorage_.yDataSpan(), atomStorage_.zDataSpan());
    neighborList_.clear();
    invalidateMetrics();
    invalidateVectorField();
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
    invalidateVectorField();
}

void World::updateVectorField() {
    if (!vectorFieldDirty_) {
        return;
    }

    vectorField_.compute(state_.forceField_, atomStorage_, grid);
    vectorFieldDirty_ = false;
}
