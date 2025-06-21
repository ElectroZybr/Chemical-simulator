#pragma once
#include <unordered_set>
#include <iostream>

class Atom;

class SpatialGrid {
private:
    std::unordered_set<Atom*>*** grid;
public:
    int sizeX;
    int sizeY;
    SpatialGrid(int sizeX, int sizeY);
    ~SpatialGrid();
    // void putMatrix5X5();
    void getMatrix5X5(int x, int y, char* matrix[5]);

    void insert(int x, int y, Atom* val) { grid[x][y]->insert(val); } // std::cout << "<Interaction> val: " << val << " | " << x << " | " << y << std::endl;
    void erase(int x, int y, Atom* val) { grid[x][y]->erase(val); }
    std::unordered_set<Atom*>* at(int x, int y) const {
        if (x >= 0 && x < sizeX && y >= 0 && y < sizeY) return grid[x][y]; 
        return nullptr; }
};