#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/NeighborSearch/NeighborList.h"
#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/metrics/EnergyMetrics.h"
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Engine/physics/ForceField.h"
#include "Engine/physics/Integrator.h"
#include "Engine/physics/VectorField.h"

class World {
public:
    explicit World(glm::vec3 size, glm::vec3 renderOffset = glm::vec3{0.0f, 0.0f, 0.0f});

    void clear();
    void reset();
    void resizeBox(const glm::vec3& newSize, float cellSize = -1.0f);
    glm::vec3 getSizeBox() const noexcept { return size; }

    void setWorldSize(const glm::vec3& newSize) {
        size = newSize;
        grid.resize(size);
        vectorField_.resize(glm::ivec3(size), 0);
        invalidateVectorField();
    }
    const glm::vec3& getWorldSize() const noexcept { return size; }

    void setRenderOffset(const glm::vec3& offset) noexcept { renderOffset = offset; }
    const glm::vec3& getRenderOffset() const noexcept { return renderOffset; }

    void setGravity(const glm::vec3& g) { gravity = g; }
    const glm::vec3& getGravity() const noexcept { return gravity; }

    void setGridCellSize(float newSize) { grid.resize(size, newSize); }
    float getGridCellSize() const noexcept { return grid.cellSize; }
    void setNeighborListCutoff(float cutoff) { neighborList_.setCutoff(cutoff); }
    float getNeighborListCutoff() const noexcept { return neighborList_.cutoff(); }
    void setNeighborListSkin(float skin) { neighborList_.setSkin(skin); }
    float getNeighborListSkin() const noexcept { return neighborList_.skin(); }
    float getNeighborListRadius() const noexcept { return neighborList_.listRadius(); }

    bool isLJEnabled() const { return ljEnabled; }
    void setLJEnabled(bool v) { ljEnabled = v; }
    bool isCoulombEnabled() const { return coulombEnabled; }
    void setCoulombEnabled(bool v) { coulombEnabled = v; }
    bool isCoulombLongRangeEnabled() const { return longRangeForcesEnabled; }
    void setCoulombLongRangeEnabled(bool v) { longRangeForcesEnabled = v; }

    AtomStorage& getAtomStorage() noexcept { return atomStorage_; }
    const AtomStorage& getAtomStorage() const noexcept { return atomStorage_; }

    Bond::List& getBonds() noexcept { return bonds_; }
    const Bond::List& getBonds() const noexcept { return bonds_; }

    SpatialGrid& getGrid() noexcept { return grid; }
    const SpatialGrid& getGrid() const noexcept { return grid; }

    NeighborList& getNeighborList() noexcept { return neighborList_; }
    const NeighborList& getNeighborList() const noexcept { return neighborList_; }

    void addAtom(const glm::vec3& start_coords, const glm::vec3& start_speed, AtomData::Type type, bool fixed);
    void addBond(size_t aIndex, size_t bIndex);
    void removeAtom(size_t atomIndex);
    void remapAtomIndices(std::span<const uint32_t> oldToNew);
    void clearAtoms() {
        atomStorage_.clear();
        invalidateVectorField();
    };
    void clearBonds() { bonds_.clear(); }
    void reserveAtoms(size_t count) { atomStorage_.reserve(count); }
    void appendAtomFast(const glm::vec3& startCoords, const glm::vec3& startSpeed, AtomData::Type type, bool fixed = false) {
        atomStorage_.addAtom(startCoords, startSpeed, type, fixed);
        invalidateMetrics();
        invalidateVectorField();
    }
    void finalizeAtomBatch();

    void setTitle(std::string_view title) { title_ = title; }
    const std::string& title() const noexcept { return title_; }
    void setDescription(std::string_view description) { description_ = description; }
    const std::string& description() const noexcept { return description_; }
    void clearMetadata() {
        title_.clear();
        description_.clear();
    }

    struct WorldState {
        Integrator integrator;
        ForceField forceField_;
        float Dt = 0.01f;
        size_t sim_step = 0;
        float sim_time_ns = 0.0f;
        bool bondFormationEnabled_ = false;
        mutable bool metricsCacheValid_ = false;
        mutable EnergyMetrics::Snapshot metricsCache_{};
    };

    WorldState& getState() noexcept { return state_; }
    const WorldState& getState() const noexcept { return state_; }

    // === Управление состоянием симуляции ===
    void update();
    
    // Параметры интегратора
    void setDt(float dt) noexcept { state_.Dt = dt; }
    float getDt() const noexcept { return state_.Dt; }
    
    Integrator& getIntegrator() noexcept { return state_.integrator; }
    const Integrator& getIntegrator() const noexcept { return state_.integrator; }
    
    ForceField& getForceField() noexcept { return state_.forceField_; }
    const ForceField& getForceField() const noexcept { return state_.forceField_; }

    VectorField& getVectorField() noexcept { return vectorField_; }
    const VectorField& getVectorField() const noexcept { return vectorField_; }
    void setVectorFieldSlice(int zSlice) {
        vectorField_.setSliceZ(zSlice);
        invalidateVectorField();
    }
    void setVectorFieldCellSize(float cellSize) {
        vectorField_.setCellScale(cellSize);
        invalidateVectorField();
    }
    void updateVectorField();
    void invalidateVectorField() noexcept { vectorFieldDirty_ = true; }
    
    // Параметры связей между атомами
    void setBondFormationEnabled(bool enabled) noexcept { state_.bondFormationEnabled_ = enabled; }
    bool isBondFormationEnabled() const noexcept { return state_.bondFormationEnabled_; }
    
    // Метрики и статистика
    void invalidateMetrics() const noexcept { state_.metricsCacheValid_ = false; }
    const EnergyMetrics::Snapshot& getMetrics() const;
    
    // Состояние симуляции
    size_t getSimStep() const noexcept { return state_.sim_step; }
    float getSimTimeNs() const noexcept { return state_.sim_time_ns; }
    void setSimStep(size_t step) noexcept { state_.sim_step = step; }
    void setSimTimeNs(float timeNs) noexcept { state_.sim_time_ns = timeNs; }
    void restoreRuntimeState(size_t step, float timeNs) noexcept {
        state_.sim_step = step;
        state_.sim_time_ns = timeNs;
    }
    void resetRuntimeState() noexcept { restoreRuntimeState(0, 0.0f); }

private:
    glm::vec3 size;
    glm::vec3 renderOffset;
    glm::vec3 gravity;

    bool ljEnabled = true;
    bool coulombEnabled = true;
    bool longRangeForcesEnabled = true;

    AtomStorage atomStorage_;
    SpatialGrid grid;
    NeighborList neighborList_;
    VectorField vectorField_;
    bool vectorFieldDirty_ = true;
    Bond::List bonds_;
    std::string title_;
    std::string description_;
    WorldState state_;
};
