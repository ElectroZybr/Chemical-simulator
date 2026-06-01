#pragma once

#include <cstdint>
#include <vector>

#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/physics/AtomData.h"

namespace Generators {
    struct AtomTypeSpec {
        AtomData::Type type;
        int absoluteCount = 0;
        float concentrationPercent = 0.0f;
    };

    /// Создает рандомный газ в уже существующем ящике симуляции
    /// @param sim Симуляция
    /// @param atomCount Количество атомов для добавления
    /// @param type Тип атома
    /// @param is3d 2D или 3D режим
    /// @param minDistance Минимальное расстояние между атомами
    /// @param speedScale Масштаб начальных скоростей атомов
    /// @param maxAttemptsPerAtom Максимальное количество попыток размещения для каждого атома
    /// @param seed Seed для рандома (0 = случайный)
    /// @return Количество успешно добавленных атомов
    int randomGasInCurrentBox(Lattice::Simulation& sim, int atomCount, AtomData::Type type, bool is3d, float minDistance = 4.0f,
                              float speedScale = 5.0f, int maxAttemptsPerAtom = 20, uint32_t seed = 0);

    /// Создает рандомный газ одного типа атомов с автоматическим подбором размера ящика
    /// @param sim Симуляция
    /// @param atomCount Общее количество атомов
    /// @param type Тип атома
    /// @param is3d 2D или 3D режим
    /// @param spacing Базовое расстояние между атомами
    /// @param margin Отступ от границ
    /// @param density Плотность газа, влияет на эффективное расстояние (0.25-3.0)
    /// @param speedScale Масштаб начальных скоростей
    /// @param seed Seed для рандома (0 = случайный)
    void randomGas(Lattice::Simulation& sim, int atomCount, AtomData::Type type, bool is3d, double spacing = 6.0, double margin = 6.0,
                   float density = 1.0f, float speedScale = 5.0f, uint32_t seed = 0);

    /// Создает смешанный рандомный газ с разными типами атомов
    /// @param sim Симуляция
    /// @param totalAtomCount Общее количество атомов для создания
    /// @param atomSpecs Вектор спецификаций типов атомов
    /// @param is3d 2D или 3D режим
    /// @param spacing Расстояние между атомами
    /// @param margin Отступ от границ
    /// @param density Плотность газа (0.25-3.0)
    /// @param speedScale Масштаб начальных скоростей
    /// @param seed Seed для рандома (0 = случайный)
    void randomGasMixed(Lattice::Simulation& sim, int totalAtomCount, const std::vector<AtomTypeSpec>& atomSpecs, bool is3d,
                        double spacing = 6.0, double margin = 6.0, float density = 1.0f, float speedScale = 5.0f,
                        uint32_t seed = 0);

    /// Создает рандомный газ на основе концентраций типов атомов
    /// @param sim Симуляция
    /// @param totalAtomCount Общее количество атомов
    /// @param concentrations Вектор спецификаций с процентами
    /// @param is3d 2D или 3D режим
    /// @param spacing Расстояние между атомами
    /// @param margin Отступ от границ
    /// @param density Плотность газа (0.25-3.0)
    /// @param speedScale Масштаб начальных скоростей
    /// @param seed Seed для рандома (0 = случайный)
    void randomGasByConcentration(Lattice::Simulation& sim, int totalAtomCount, const std::vector<AtomTypeSpec>& concentrations, bool is3d,
                                  double spacing = 6.0, double margin = 6.0, float density = 1.0f, float speedScale = 5.0f,
                                  uint32_t seed = 0);
}
