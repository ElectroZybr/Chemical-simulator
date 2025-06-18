#pragma once

class Atom;

class SpatialGrid {
private:
    Atom*** grid;
public:
    int sizeX;
    int sizeY;
    SpatialGrid(int sizeX, int sizeY);
    ~SpatialGrid();
    // void putMatrix5X5();
    void getMatrix5X5(int x, int y, char* matrix[5]);

    void put(int x, int y, Atom* val) { grid[x][y] = val; }
    Atom* at(int x, int y) const { 
        if (x >= 0 && x < sizeX && y >= 0 && y < sizeY) return grid[x][y]; 
        return nullptr; }
};