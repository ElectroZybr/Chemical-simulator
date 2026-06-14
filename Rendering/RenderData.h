#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

struct RenderColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct RenderAtomsView {
    size_t count = 0;

    const float* x = nullptr;
    const float* y = nullptr;
    const float* z = nullptr;

    const float* vx = nullptr;
    const float* vy = nullptr;
    const float* vz = nullptr;

    const uint8_t* type = nullptr;
    const float* radius = nullptr;

    [[nodiscard]] bool empty() const noexcept { return count == 0; }
    [[nodiscard]] bool hasPositions() const noexcept { return x != nullptr && y != nullptr && z != nullptr; }
    [[nodiscard]] bool hasVelocities() const noexcept { return vx != nullptr && vy != nullptr && vz != nullptr; }
    [[nodiscard]] bool hasTypes() const noexcept { return type != nullptr; }
    [[nodiscard]] bool hasRadii() const noexcept { return radius != nullptr; }
};

struct RenderBond {
    size_t aIndex = 0;
    size_t bIndex = 0;
};

struct RenderGridCell {
    glm::vec3 origin{};
    float cellSize = 1.0f;
    float atomCount = 0.0f;
};

using RenderBondVisitor = void (*)(size_t aIndex, size_t bIndex, void* userData);
using RenderGridCellVisitor = void (*)(const RenderGridCell& cell, void* userData);

struct RenderBondsView {
    const void* context = nullptr;
    size_t count = 0;
    void (*forEachFn)(const void* context, RenderBondVisitor visitor, void* userData) = nullptr;

    [[nodiscard]] bool empty() const noexcept { return count == 0 || context == nullptr || forEachFn == nullptr; }
    void forEach(RenderBondVisitor visitor, void* userData) const {
        if (!empty()) {
            forEachFn(context, visitor, userData);
        }
    }
};

struct RenderGridView {
    const void* context = nullptr;
    size_t count = 0;
    void (*forEachFn)(const void* context, RenderGridCellVisitor visitor, void* userData) = nullptr;

    [[nodiscard]] bool empty() const noexcept { return count == 0 || context == nullptr || forEachFn == nullptr; }
    void forEach(RenderGridCellVisitor visitor, void* userData) const {
        if (!empty()) {
            forEachFn(context, visitor, userData);
        }
    }
};

struct RenderVectorFieldView {
    const float* values = nullptr;
    const glm::vec2* vectors = nullptr;
    glm::ivec2 gridSize{0, 0};
    glm::ivec2 coverageSize{0, 0};
    float cellSize = 1.0f;
    float z = 0.0f;

    [[nodiscard]] bool empty() const noexcept { return values == nullptr || gridSize.x < 2 || gridSize.y < 2; }
    [[nodiscard]] size_t cellCount() const noexcept {
        if (empty()) {
            return 0;
        }
        return static_cast<size_t>(gridSize.x - 1) * static_cast<size_t>(gridSize.y - 1);
    }
    [[nodiscard]] float valueAt(int x, int y) const noexcept {
        return values[x + gridSize.x * y];
    }
    [[nodiscard]] glm::vec2 vectorAt(int x, int y) const noexcept {
        return vectors == nullptr ? glm::vec2(0.0f) : vectors[x + gridSize.x * y];
    }
    [[nodiscard]] size_t vectorCount() const noexcept {
        return empty() ? 0 : static_cast<size_t>(gridSize.x) * static_cast<size_t>(gridSize.y);
    }
};

class RenderData {
public:
    enum class SpeedColorMode : uint8_t {
        AtomColor = 0,
        GradientClassic = 1,
        GradientTurbo = 2,
    };

    RenderAtomsView atoms{};

    RenderBondsView bonds{};
    RenderGridView grid{};
    RenderVectorFieldView vectorField{};
    std::vector<size_t> selectedAtomIndices;

    glm::vec3 worldSize{0.0f, 0.0f, 0.0f};
    glm::vec3 renderOffset{0.0f, 0.0f, 0.0f};

    bool isActiveWorld = false;
    bool hasBox = false;
    bool drawAtoms = true;
    bool drawGrid = false;
    bool drawVectorField = false;
    bool drawFieldArrows = false;
    bool drawFieldContours = false;
    bool drawBonds = false;
    bool drawBox = true;
    bool drawMemoryOrder = false;
    bool fieldAutoScale = true;
    SpeedColorMode speedColorMode = SpeedColorMode::AtomColor;
    float speedGradientMax = 5.0f;
    float fieldPotentialScale = 50.0f;
    float fieldCellSize = 1.0f;
    float fieldSmoothing = 1.0f;
    float fieldContourStep = 0.2f;
    float alpha = 0.05f;
};
