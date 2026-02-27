#include "SpatialGrid.h"

#include <cmath>

SpatialGrid::SpatialGrid(int sizeX, int sizeY, float cellSize)
    : sizeX(sizeX),
      sizeY(sizeY),
      cellSize(cellSize > 0.0f ? cellSize : 1.0f),
      invCellSize(1.0f / (cellSize > 0.0f ? cellSize : 1.0f)) {
    cellsX = std::max(1, static_cast<int>(std::ceil(sizeX * invCellSize)));
    cellsY = std::max(1, static_cast<int>(std::ceil(sizeY * invCellSize)));

    grid = new std::unordered_set<Atom*>**[cellsX];
    for (int i = 0; i < cellsX; ++i) {
        grid[i] = new std::unordered_set<Atom*>*[cellsY];
        for (int j = 0; j < cellsY; ++j) {
            grid[i][j] = new std::unordered_set<Atom*>;
        }
    }
}

SpatialGrid::~SpatialGrid() {
    for (int i = 0; i < cellsX; ++i) {
        for (int j = 0; j < cellsY; ++j) {
            delete grid[i][j];
        }
        delete[] grid[i];
    }
    delete[] grid;
}
