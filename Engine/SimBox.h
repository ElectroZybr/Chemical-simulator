#pragma once

#include "math\Vec3D.h"
#include "physics\SpatialGrid.h"

class Renderer;

class SimBox {
    public:
        SimBox(Vec3D start, Vec3D end);
        void setRenderer(Renderer* r);
        bool setSizeBox(Vec3D s = Vec3D(0, 0, 0), Vec3D e = Vec3D(50, 50, 3), int cellSize = -1);
        SpatialGrid grid;
        Vec3D start;
        Vec3D end;
    private:
        Renderer* render = nullptr;
};
