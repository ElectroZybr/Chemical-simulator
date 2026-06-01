#pragma once

#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/physics/AtomData.h"

namespace Generators {
    /// Создает треугольную биапирамидальную кристаллическую структуру
    /// @param sim Симуляция
    /// @param baseSideAtoms Количество атомов в одной стороне максимального треугольного слоя
    /// @param type Тип атома
    /// @param verticalScale Масштаб вертикального расстояния между слоями
    /// @param spacing Расстояние между атомами (0.0 = автоматическое на основе LJ параметров)
    /// @param margin Отступ от границ симуляционного ящика
    void triangularBipyramidCrystal(Lattice::Simulation& sim, int baseSideAtoms, AtomData::Type type, float verticalScale = 1.0f,
                                    double spacing = 0.0, double margin = 12.0);
}
