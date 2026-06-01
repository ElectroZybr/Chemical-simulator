#pragma once

#include <cstdint>
#include <variant>

class AtomStorage;
class ForceField;
class NeighborList;
class World;

#include "Engine/physics/integrators/KDKScheme.h"
#include "Engine/physics/integrators/LangevinScheme.h"
#include "Engine/physics/integrators/RK4Scheme.h"
#include "Engine/physics/integrators/VerletScheme.h"
#include "Engine/physics/integrators/Andersen.h"

struct StepData {
    World& world;
    ForceField& forceField;
    NeighborList& neighborList;
    bool allowBondFormation;
    bool bondsChanged = false;
    float accelDamping;
    float dt;
};

class Integrator {
public:
    enum class Scheme : uint8_t {
        Verlet,   // классический Velocity Verlet: устойчивый и быстрый базовый выбор
        KDK,      // Kick-Drift-Kick: симплектическая схема, удобна для поэтапного обновления сил
        RK4,      // Runge-Kutta 4-го порядка: высокая точность на шаг, но дороже по вычислениям
        Langevin, // стохастический интегратор с термостатом (трение + случайный шум)
        Andersen, // термостат с мк шагами для поддержания постоянной температуры
    };

    Integrator();

    void setScheme(Scheme scheme);
    Scheme getScheme() const { return integrator_type; }
    void setMaxParticleSpeed(float maxSpeed);
    float maxParticleSpeed() const { return maxParticleSpeed_; }
    void setAccelDamping(float accelDamping);
    float accelDamping() const { return accelDamping_; }
    void setAndersenTemperature(float temperature);
    float andersenTemperature() const;

    void step(StepData& stepData);

private:
    using SchemeVariant = std::variant<VerletScheme, KDKScheme, RK4Scheme, LangevinScheme, Andersen>;

    SchemeVariant makeSchemeImpl(Scheme scheme) const;

    Scheme integrator_type = Scheme::Verlet;
    SchemeVariant scheme_impl;
    float maxParticleSpeed_ = 0.0f;
    float accelDamping_ = 0.9f;
    float andersenTemperature_ = 300.0f;
};
