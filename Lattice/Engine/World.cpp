#include "World.h"

#include "Lattice/Engine/metrics/EnergyMetrics.h"
#include "Lattice/Engine/physics/BondOps.h"
#include "Lattice/Engine/physics/IIntegrator.h"
#include "Lattice/Log.hpp"

World::World(glm::vec3 size, glm::vec3 renderOffset) : size(size), renderOffset(renderOffset), grid(size), vectorField_(glm::ivec3(size), 0) {
    atomStorage_.reserve(250000);
    neighborList_.setParams(5.f, 1.f);
    if (!state_.forceField_.setForceField("classic_md")) {
        Log::error("World", "Failed to select default force field 'classic_md'");
    }
    else if (state_.forceField_.activeForceField() == nullptr) {
        Log::warning("World", "Default force field 'classic_md' is selected but not available");
    }
    else {
        Log::ok("World", "Default force field selected: {}", state_.forceField_.getForceField());
    }
    state_.thermostat.setParam(100.0f);
    state_.thermostat.setThermostat("barendsen");
}

void World::clear() {
    clearAtoms();
    clearBonds();
    neighborList_.clear();
    grid.rebuild(atomStorage_.x(), atomStorage_.y(), atomStorage_.z());
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
    (void)atomStorage_.addAtom(start_coords, start_speed, type, fixed, AtomData::Hybridization::None);
    grid.rebuild(atomStorage_.x(), atomStorage_.y(), atomStorage_.z());
    invalidateMetrics();
    invalidateVectorField();
}

void World::addBond(size_t aIndex, size_t bIndex, uint8_t order, const ChemistryData& chemistryData) { BondOps::create(bonds_, aIndex, bIndex, order, atomStorage_, chemistryData); }

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
    removeAtoms({atomIndex});
}

void World::removeAtoms(std::vector<size_t> atomIndices) {
    if (atomIndices.empty()) {
        return;
    }

    std::sort(atomIndices.begin(), atomIndices.end());
    atomIndices.erase(std::unique(atomIndices.begin(), atomIndices.end()), atomIndices.end());

    for (auto itIndex = atomIndices.rbegin(); itIndex != atomIndices.rend(); ++itIndex) {
        const size_t atomIndex = *itIndex;
        if (atomIndex >= atomStorage_.size()) {
            continue;
        }

        const size_t lastIndex = atomStorage_.size() - 1;

        for (auto it = bonds_.begin(); it != bonds_.end();) {
            if (it->aIndex == atomIndex || it->bIndex == atomIndex) {
                const uint8_t valenceCost = static_cast<uint8_t>(it->order + 1);
                if (it->aIndex == atomIndex && it->bIndex != atomIndex && it->bIndex < atomStorage_.size()) {
                    atomStorage_.valence()[it->bIndex] += valenceCost;
                }
                if (it->bIndex == atomIndex && it->aIndex != atomIndex && it->aIndex < atomStorage_.size()) {
                    atomStorage_.valence()[it->aIndex] += valenceCost;
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
    }
    grid.rebuild(atomStorage_.x(), atomStorage_.y(), atomStorage_.z());
    neighborList_.clear();
    invalidateMetrics();
    invalidateVectorField();
}

void World::setAtomsFixed(std::span<const AtomStorage::AtomId> atomIds, bool fixed) {
    if (atomIds.empty() || atomStorage_.empty()) {
        return;
    }

    std::vector<AtomStorage::AtomId> previousOrder(atomStorage_.size(), AtomStorage::InvalidAtomId);
    for (size_t index = 0; index < atomStorage_.size(); ++index) {
        previousOrder[index] = atomStorage_.atomId(index);
    }

    for (const AtomStorage::AtomId atomId : atomIds) {
        const size_t index = atomStorage_.indexOf(atomId);
        if (index < atomStorage_.size()) {
            atomStorage_.setFixed(index, fixed);
        }
    }

    std::vector<uint32_t> oldToNew(atomStorage_.size(), 0);
    for (size_t oldIndex = 0; oldIndex < previousOrder.size(); ++oldIndex) {
        const size_t newIndex = atomStorage_.indexOf(previousOrder[oldIndex]);
        oldToNew[oldIndex] = static_cast<uint32_t>(newIndex < atomStorage_.size() ? newIndex : oldIndex);
    }
    remapAtomIndices(oldToNew);

    grid.rebuild(atomStorage_.x(), atomStorage_.y(), atomStorage_.z());
    neighborList_.clear();
    invalidateMetrics();
    invalidateVectorField();
}

void World::finalizeAtomBatch() {
    grid.rebuild(atomStorage_.x(), atomStorage_.y(), atomStorage_.z());
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
    StepContext stepContext{
        .world = *this,
        .forceField = state_.forceField_,
        .neighborList = neighborList_,
        .thermostat = state_.thermostat.activeThermostat(),
        .chemistryData = state_.chemistryData,
        .allowBondFormation = state_.bondFormationEnabled_,
        .bondsChanged = false,
        .dt = state_.Dt,
    };

    // Выполнить шаг интеграции
    state_.integrator.step(stepContext);

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

    const IForceField* forceField = state_.forceField_.activeForceField();
    if (forceField == nullptr) {
        Log::warning("World", "Vector field update skipped: no active force field");
        vectorField_.compute(nullptr, atomStorage_, grid);
        vectorFieldDirty_ = false;
        return;
    }

    vectorField_.compute(forceField, atomStorage_, grid);
    vectorFieldDirty_ = false;
    Log::info("World", "Vector field updated for force field {}", state_.forceField_.getForceField());
}
