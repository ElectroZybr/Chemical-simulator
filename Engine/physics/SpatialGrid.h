#pragma once

class SpatialGrid {
private:
    char** grid;
public:
    int sizeX;
    int sizeY;
    SpatialGrid(int sizeX, int sizeY);
    ~SpatialGrid();
    // void putMatrix5X5();
    void getMatrix5X5(int x, int y, char* matrix[5]);

    void put(int x, int y, char val) { grid[x][y] = val; }
    const char& at(int x, int y) const { return grid[x][y]; }
};