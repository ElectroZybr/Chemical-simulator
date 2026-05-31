#pragma once

#include <cstdint>
#include <vector>

#include "Engine/Simulation.h"
#include "Engine/physics/AtomData.h"

namespace Scenes {
    enum class CrystalPlane : uint8_t {
        XY,
        XZ,
        YZ,
    };

    struct AtomTypeSpec {
        AtomData::Type type;
        int absoluteCount = 0;
        float concentrationPercent = 0.0f;
    };

    /// Создает кристаллическую структуру атомов
    /// @param sim Симуляция
    /// @param n Количество атомов по каждой оси
    /// @param type Тип атома
    /// @param is3d 2D или 3D режим
    /// @param padding Расстояние между атомами в сетке
    /// @param margin Отступ от границ симуляционного ящика
    void crystal(Simulation& sim, int n, AtomData::Type type, bool is3d, CrystalPlane plane = CrystalPlane::XY,
                 double padding = 3.0, double margin = 15.0);

    /// @param baseSideAtoms Количество атомов в одной стороне максимального треугольного слоя
    void triangularBipyramidCrystal(Simulation& sim, int baseSideAtoms, AtomData::Type type, float verticalScale = 1.0f,
                                    double spacing = 0.0, double margin = 12.0);

    void AngularVelocity(Simulation& sim, Vec3f angularVelocity);

    void hexLattice(Simulation& sim, Vec3f count, AtomData::Type type, float start_force = 1.0f, float margin = 15.0);

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
    int randomGasInCurrentBox(Simulation& sim, int atomCount, AtomData::Type type, bool is3d, float minDistance = 4.0f,
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
    void randomGas(Simulation& sim, int atomCount, AtomData::Type type, bool is3d, double spacing = 6.0, double margin = 6.0,
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
    void randomGasMixed(Simulation& sim, int totalAtomCount, const std::vector<AtomTypeSpec>& atomSpecs, bool is3d,
                        double spacing = 6.0, double margin = 6.0, float density = 1.0f, float speedScale = 5.0f,
                        uint32_t seed = 0);

    void randomGasByConcentration(Simulation& sim, int totalAtomCount, const std::vector<AtomTypeSpec>& concentrations, bool is3d,
                                  double spacing = 6.0, double margin = 6.0, float density = 1.0f, float speedScale = 5.0f,
                                  uint32_t seed = 0);
}
