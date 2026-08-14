#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

#include "Lattice/Engine/StepContext.h"
#include "Lattice/Engine/ModuleRegistry.hpp"

class AtomStorage;
class ForceField;
class NeighborList;
class World;

/// абстрактный интегратор
class IIntegrator : public IModule {
public:
    virtual ~IIntegrator() = default;
    virtual void step(StepContext& stepContext) = 0;
};

class Integrator : public RegisteredModuleOwner<Integrator, IIntegrator> {
public:
    Integrator() { setIntegrator("verlet"); }
    Integrator(const Integrator&) = delete;
    Integrator& operator=(const Integrator&) = delete;
    Integrator(Integrator&&) noexcept = default;
    Integrator& operator=(Integrator&&) noexcept = default;

    static ModuleRegistry2<IIntegrator>& registry();

    bool setIntegrator(std::string_view id) { return set(id); }
    std::string_view getIntegrator() const { return get(); }
    void setMaxParticleSpeed(float maxSpeed) { maxParticleSpeed_ = std::max(0.0f, maxSpeed); }
    float maxParticleSpeed() const { return maxParticleSpeed_; }
    void step(StepContext& stepContext) {
        if (!impl_) {
            return;
        }
        impl_->step(stepContext);
    }
private:
    float maxParticleSpeed_ = 0.0f;
};
