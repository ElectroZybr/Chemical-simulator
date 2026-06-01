#pragma once

#include <cstdint>

#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/physics/AtomData.h"

namespace Generators {
    enum class CrystalPlane : uint8_t {
        XY,
        XZ,
        YZ,
    };

    /// Создает кристаллическую структуру атомов
    /// @param sim Симуляция
    /// @param n Количество атомов по каждой оси
    /// @param type Тип атома
    /// @param is3d 2D или 3D режим
    /// @param padding Расстояние между атомами в сетке
    /// @param margin Отступ от границ симуляционного ящика
    void massive(Lattice::Simulation& sim, int n, AtomData::Type type, bool is3d, CrystalPlane plane = CrystalPlane::XY,
                 double padding = 3.0, double margin = 15.0);

    /// Применяет угловую скорость всем мобильным атомам
    /// @param sim Симуляция
    /// @param angularVelocity Вектор угловой скорости
    void AngularVelocity(Lattice::Simulation& sim, Vec3f angularVelocity);
}
