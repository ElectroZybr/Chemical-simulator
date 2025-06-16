#include "SpatialGrid.h"

// char SpatialGrid::grid[5000][2500] = {0};
SpatialGrid::SpatialGrid(int sizeX, int sizeY) : sizeX(sizeX), sizeY(sizeY) {
    grid = new char*[sizeX];

    for (int i = 0; i < sizeX; ++i) {
        grid[i] = new char[sizeY]();  // Зануляем память
    }

    for (int i = 0; i < sizeY; ++i) {
        for (int j = 0; j < sizeX; ++j) {
            if (i == 0 || i == sizeY-1)
                grid[i][j] = 255;
            else if (j == 0 || j == sizeX-1)
                grid[i][j] = 255;
        }
    }
}

SpatialGrid::~SpatialGrid() {
    // 1. Удаляем каждую строку
    for (int i = 0; i < sizeX; ++i) {
        delete[] grid[i];
    }

    // 2. Удаляем массив указателей
    delete[] grid;
}

void SpatialGrid::getMatrix5X5(int x, int y, char* matrix[5]) {
    for (int i = 0; i < 5; ++i) {
        matrix[i] = &grid[x + i][y];
    }
}