#include "SpatialGrid.h"

#include <cmath>

SpatialGrid::SpatialGrid(int sizeX, int sizeY, int cellSize)
    : sizeX(sizeX),
      sizeY(sizeY),
      cellSize(cellSize) {

    grid = new std::unordered_set<Atom*>**[sizeX];
    for (int i = 0; i < sizeX; ++i) {
        grid[i] = new std::unordered_set<Atom*>*[sizeY];
        for (int j = 0; j < sizeY; ++j) {
            grid[i][j] = new std::unordered_set<Atom*>;
        }
    }
}

SpatialGrid::~SpatialGrid() {
    for (int i = 0; i < sizeX; ++i) {
        for (int j = 0; j < sizeY; ++j) {
            delete grid[i][j];
        }
        delete[] grid[i];
    }
    delete[] grid;
}
