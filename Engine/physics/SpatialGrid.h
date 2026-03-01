#pragma once
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <cmath>

class Atom;

class SpatialGrid {
private:
    std::unordered_set<Atom*>*** grid;
    void clearGridMemory();
    void allocateGridMemory(int newSizeX, int newSizeY);
public:
    int sizeX;
    int sizeY;
    int cellSize;

    SpatialGrid(int sizeX, int sizeY, int cellSize = 3);
    ~SpatialGrid();
    void resize(int newSizeX, int newSizeY, int newCellSize = -1);

    void insert(int x, int y, Atom* val) {
        if (auto cell = at(x, y)) {
            cell->insert(val);
        }
    }

    void erase(int x, int y, Atom* val) {
        if (auto cell = at(x, y)) {
            cell->erase(val);
        }
    }

    std::unordered_set<Atom*>* at(int x, int y) const {
        if (x >= 0 && x < sizeX && y >= 0 && y < sizeY) return grid[x][y];
        return nullptr; }

    int worldToCellX(double x) const {
        if (x < 0.0) return -1;
        return static_cast<int>(x / cellSize);
    }

    int worldToCellY(double y) const {
        if (y < 0.0) return -1;
        return static_cast<int>(y / cellSize);
    }
};
