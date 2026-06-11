#include "VectorField.h"

#include "Engine/NeighborSearch/SpatialGrid.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/ForceField.h"

#include <algorithm>
#include <cmath>
#include <iostream>

VectorField::VectorField(glm::ivec3 size, int sliceZ)
    : size(0), domain(1), sliceZ(0) {
    resize(size, sliceZ);
}

void VectorField::resize(glm::ivec3 newDomainSize, int newSliceZ, float newScale) {
    domain = glm::max(newDomainSize, glm::ivec3(1));
    scale = std::max(newScale, 0.1f);
    const int nodeCountX = static_cast<int>(std::ceil(static_cast<float>(domain.x) / scale)) + 1;
    const int nodeCountY = static_cast<int>(std::ceil(static_cast<float>(domain.y) / scale)) + 1;
    size = glm::ivec3(std::max(2, nodeCountX), std::max(2, nodeCountY), std::max(1, domain.z));
    field.assign(static_cast<size_t>(size.x) * static_cast<size_t>(size.y), 0.0f);
    vectorField.assign(field.size(), glm::vec2(0.0f));
    setSliceZ(newSliceZ);
}

void VectorField::setCellScale(float newScale) {
    resize(domain, sliceZ, newScale);
}

void VectorField::setSliceZ(int newSliceZ) {
    sliceZ = std::clamp(newSliceZ, 0, std::max(0, domain.z));
}

void VectorField::compute(ForceField& forceField, AtomStorage& atoms, SpatialGrid& grid) {
    const float z = static_cast<float>(sliceZ);
    for (int y = 0; y < size.y; ++y) {
        for (int x = 0; x < size.x; ++x) {
            const float sampleX = std::min(static_cast<float>(x) * scale, static_cast<float>(domain.x));
            const float sampleY = std::min(static_cast<float>(y) * scale, static_cast<float>(domain.y));
            const auto sample = forceField.coulombForceField_.fieldAtPoint(atoms, grid, sampleX, sampleY, z);
            const int index = x + size.x * y;
            field[index] = sample.potential;
            vectorField[index] = glm::vec2(sample.field.x, sample.field.y);
        }
    }
}

void VectorField::show() const {
    for (int y = 0; y < size.y; ++y) {
        for (int x = 0; x < size.x; ++x) {
            std::cout << potentialAt(x, y) << " ";
        }
        std::cout << "\n";
    }
    std::cout << "Layer " << sliceZ << " end\n";
}
