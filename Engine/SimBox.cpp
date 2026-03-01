#include "SimBox.h"
#include "Renderer.h"

SimBox::SimBox(Vec3D start, Vec3D end) : start(start), end(end), grid(static_cast<int>(end.x - start.x), static_cast<int>(end.y - start.y)) {}

void SimBox::setRenderer(Renderer* r) {
    render = r;
    setSizeBox(start, end);
}

bool SimBox::setSizeBox(Vec3D s, Vec3D e, int cellSize) {
    /* Если сетка обновилась метод возвращает True*/
    bool flag = false;
    if (static_cast<int>(e.x-s.x) != static_cast<int>(end.x-start.x) || static_cast<int>(e.y-s.y) != static_cast<int>(end.y-start.y)) {
        grid.resize(static_cast<int>(e.x-s.x), static_cast<int>(e.y-s.y), cellSize);
        if (render) render->wallImage(s, e);
        flag = true;
    }
    start = s;
    end = e;
    return flag;
}
