#include "SimBox.h"

#include "Engine/Consts.h"

SimBox::SimBox(Vec3f size) : size(size), grid(size) {}

bool SimBox::setSizeBox(const Vec3f& newSize, int cellSize) {
    bool resized = false;

    const bool sizeChanged = (newSize - size).sqrAbs() > Consts::Epsilon;
    const bool cellSizeChanged = (cellSize > 0 && cellSize != grid.cellSize);

    if (sizeChanged || cellSizeChanged) {
        grid.resize(newSize, cellSize);
        resized = true;
    }

    size = newSize;
    return resized;
}
