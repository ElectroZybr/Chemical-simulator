#include "Integrator.h"

#include <algorithm>

#include "Engine/physics/integrators/StepOps.h"

Integrator::Integrator() : integrator_type(Scheme::Verlet), scheme_impl(makeSchemeImpl(Scheme::Verlet)) {}

void Integrator::setScheme(Scheme scheme) {
    integrator_type = scheme;
    scheme_impl = makeSchemeImpl(scheme);
}

void Integrator::setMaxParticleSpeed(float maxSpeed) { maxParticleSpeed_ = std::max(0.0f, maxSpeed); }

void Integrator::setAccelDamping(float accelDamping) { accelDamping_ = std::clamp(accelDamping, 0.0f, 1.0f); }

void Integrator::setAndersenTemperature(float temperature) {
    andersenTemperature_ = std::max(0.0f, temperature);
    if (auto* scheme = std::get_if<Andersen>(&scheme_impl)) {
        scheme->setTemperature(andersenTemperature_);
    }
}

float Integrator::andersenTemperature() const {
    if (const auto* scheme = std::get_if<Andersen>(&scheme_impl)) {
        return static_cast<float>(scheme->getTemperature());
    }
    return andersenTemperature_;
}

void Integrator::step(StepData& stepData) {
    std::visit([&](auto& scheme) { scheme.pipeline(stepData); }, scheme_impl);

    // Ограничение максимальной скорости атомов
    if (maxParticleSpeed_ > 0.0f) {
        StepOps::postProcessVelocities(stepData.world.getAtomStorage(), maxParticleSpeed_);
    }
}

Integrator::SchemeVariant Integrator::makeSchemeImpl(Scheme scheme) const {
    switch (scheme) {
    case Scheme::Verlet:
        return VerletScheme{};
    case Scheme::KDK:
        return KDKScheme{};
    case Scheme::RK4:
        return RK4Scheme{};
    case Scheme::Langevin:
        return LangevinScheme{};
    case Scheme::Andersen:
        return Andersen{andersenTemperature_, 0.1};
    default:
        return VerletScheme{};
    }
}
