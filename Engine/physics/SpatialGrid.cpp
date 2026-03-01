#include "SpatialGrid.h"

#include <cmath>

SpatialGrid::SpatialGrid(int sizeX, int sizeY, int cellSize)
    : sizeX(sizeX),
      sizeY(sizeY),
      cellSize(cellSize) {
    allocateGridMemory(sizeX, sizeY);
}

void SpatialGrid::allocateGridMemory(int newSizeX, int newSizeY) {
    grid = new std::unordered_set<Atom*>**[newSizeX];
    for (int i = 0; i < newSizeX; ++i) {
        grid[i] = new std::unordered_set<Atom*>*[newSizeY];
        for (int j = 0; j < newSizeY; ++j) {
            grid[i][j] = new std::unordered_set<Atom*>;
        }
    }
}

void SpatialGrid::clearGridMemory() {
    if (!grid) return;

    for (int i = 0; i < sizeX; ++i) {
        for (int j = 0; j < sizeY; ++j) {
            delete grid[i][j];
        }
        delete[] grid[i];
    }
    delete[] grid;
    grid = nullptr;
}

SpatialGrid::~SpatialGrid() {
    clearGridMemory();
}

void SpatialGrid::resize(int newSizeX, int newSizeY, int newCellSize) {
    clearGridMemory();

    sizeX = newSizeX;
    sizeY = newSizeY;
    if (newCellSize > 0) {
        cellSize = newCellSize;
    }

    allocateGridMemory(sizeX, sizeY);
}
