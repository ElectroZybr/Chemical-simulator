#pragma once
#include <unordered_set>
#include <iostream>
#include <algorithm>

class Atom;

class SpatialGrid {
private:
    std::unordered_set<Atom*>*** grid;
public:
    int sizeX;
    int sizeY;
    float cellSize;
    float invCellSize;
    int cellsX;
    int cellsY;

    SpatialGrid(int sizeX, int sizeY, float cellSize = 1.0f);
    ~SpatialGrid();
    // void putMatrix5X5();
    void getMatrix5X5(int x, int y, char* matrix[5]);

    void insert(int x, int y, Atom* val) { grid[x][y]->insert(val); } // std::cout << "<Interaction> val: " << val << " | " << x << " | " << y << std::endl;
    void erase(int x, int y, Atom* val) { grid[x][y]->erase(val); }
    std::unordered_set<Atom*>* at(int x, int y) const {
        if (x >= 0 && x < cellsX && y >= 0 && y < cellsY) return grid[x][y];
        return nullptr; }

    int worldToCellX(double x) const {
        if (x < 0.0) return -1;
        return static_cast<int>(x * invCellSize);
    }

    int worldToCellY(double y) const {
        if (y < 0.0) return -1;
        return static_cast<int>(y * invCellSize);
    }

    int worldRadiusToCellRange(double worldRadius) const {
        int r = static_cast<int>(worldRadius * invCellSize) + 1;
        return std::max(1, r);
    }
};
