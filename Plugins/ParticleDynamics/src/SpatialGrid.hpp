#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <stdexcept>
#include <cmath>

#include <glm/vec3.hpp>

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>
#include <Lattice/Kernel/Restrict.hpp>
#include <Lattice/Tools/Logger.hpp>

// Plugin dependences
#include "ParticleDynamics/api/ParticleAPI.hpp"
#include "ParticleDynamics/api/ParticleStorage.hpp"

#include "CSR.hpp"

namespace ParticleDynamics {

class SpatialGrid final : public SpatialIndexAPI {
public:
    explicit SpatialGrid(Lattice::Components& components) {
        Lattice::Component settings = components.require<Lattice::Settings>();
        settings->bind("SpatialGrid", "size", &size, [this](glm::vec3 newSize) { setSize(newSize); });
        settings->bind("SpatialGrid", "cell_size", &cellSize, [this](float value) { setCellSize(value); });

        particles = components.require<ParticleStorage>();
        Logger::info("SpatialGrid", "initialized");
    }

    void configure() {
        
    }

    void rebuild() override {
        const size_t n = particles->size();

        if (n == 0) {
            buffer_.clear();
            return;
        }

        cellIndices_.resize(n);
        counts_.assign(countCells, 0);

        float* RESTRICT x = particles->getCol<Pos::X>();
        float* RESTRICT y = particles->getCol<Pos::Y>();
        float* RESTRICT z = particles->getCol<Pos::Z>();

        for (size_t i = 0; i < n; ++i) {
            const uint32_t cx = static_cast<uint32_t>(x[i] * invCellSize) + 1;
            const uint32_t cy = static_cast<uint32_t>(y[i] * invCellSize) + 1;
            const uint32_t cz = static_cast<uint32_t>(z[i] * invCellSize) + 1;
            const uint32_t cell = (cz * size.y + cy) * size.x + cx;

            cellIndices_[i] = cell;
            ++counts_[cell];
        }

        buffer_.resize(n, countCells + 1);
        uint32_t running = 0;
        // size_t nonEmptyCellCount = 0;
        uint32_t maxAtomsPerCell = 0;
        for (size_t cell = 0; cell < countCells; ++cell) {
            const uint32_t cnt = counts_[cell];
            // nonEmptyCellCount += (cnt > 0);
            maxAtomsPerCell = std::max(counts_[cell], maxAtomsPerCell);
            counts_[cell] = running;
            buffer_.offsets()[cell] = running;
            running += cnt;
        }
        buffer_.offsets()[countCells] = running;

        for (size_t i = 0; i < n; ++i) {
            const size_t cell = cellIndices_[i];
            buffer_.values()[counts_[cell]++] = i;
        }
    }

    [[nodiscard]] std::span<uint32_t> particlesInCell(uint32_t x, uint32_t y, uint32_t z) {
        return particlesInCell((z * size.y + y) * size.x + x);
    }

    [[nodiscard]] std::span<uint32_t> particlesInCell(size_t linearIndex) {
        return buffer_[linearIndex];
    }

    void rebuildNeighborOffsets() noexcept {
        /* построение массива смещений для 27 соседей */
        uint32_t k = 0;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    neighborOffsets27_[k++] = dx + (dy + dz * size.y) * size.x;
                }
            }
        }
    }

    size_t memoryBytes() const {
        return buffer_.memoryBytes() + sizeof(neighborOffsets27_) +
            cellIndices_.capacity() * sizeof(cellIndices_[0]) + counts_.capacity() * sizeof(counts_[0]);
    }

    ~SpatialGrid() {

    }

private:
    Lattice::Component<ParticleStorage> particles;

    glm::vec3 size{};
    float cellSize = 5.0f;
    float invCellSize = 0.2f;
    uint32_t ghostLayers = 1;
    
    size_t countCells = 0;
    
    // CSR хранение данных
    CSR<uint32_t> buffer_;
    std::array<int, 27> neighborOffsets27_{};

    std::vector<uint32_t> cellIndices_;
    std::vector<uint32_t> counts_;

    void setCellSize(float newCellSize) {
        if (newCellSize <= 0) {
            throw std::invalid_argument("SpatialGrid::resize: newCellSize must be > 0");
        }

        invCellSize = 1.0f / newCellSize;

        buffer_.clear();
        rebuildNeighborOffsets();
    }

    void setSize(glm::vec3 newSize) {
        auto calculateCells = [this](float worldDim) -> uint32_t {
            float num = std::ceil(worldDim * invCellSize);
            return static_cast<uint32_t>(std::max(1.0f, num));
        };

        uint32_t ghostPadding = ghostLayers * 2;
        size.x = calculateCells(newSize.x) + ghostPadding;
        size.y = calculateCells(newSize.y) + ghostPadding;
        size.z = calculateCells(newSize.z) + ghostPadding;
        countCells = size.x * size.y * size.z;

        buffer_.clear();
        rebuildNeighborOffsets();
    }
};
}