#pragma once

#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/physics/AtomData.h"

namespace Generators {
    /// Создает шестиугольную кристаллическую решетку (FCC или близко к ней)
    /// @param sim Симуляция
    /// @param count Количество атомов в каждом измерении
    /// @param type Тип атома
    /// @param start_force Начальная величина случайной скорости
    /// @param margin Отступ от границ симуляционного ящика
    void hexLattice(Lattice::Simulation& sim, Vec3f count, AtomData::Type type, float start_force = 1.0f, float margin = 15.0);
}
