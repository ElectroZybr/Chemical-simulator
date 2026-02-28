#pragma once
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <cmath>

class Atom;

class SpatialGrid {
private:
    std::unordered_set<Atom*>*** grid;
public:
    int sizeX;
    int sizeY;
    int cellSize;

    SpatialGrid(int sizeX, int sizeY, int cellSize = 3);
    ~SpatialGrid();

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
        // int curr_x = worldToCellX(x), curr_y = worldToCellY(y);
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



    // int worldRadiusToCellRange(double worldRadius) const {
    //     int r = static_cast<int>(worldRadius * invCellSize) + 1;
    //     return std::max(1, r);
    // }
};
