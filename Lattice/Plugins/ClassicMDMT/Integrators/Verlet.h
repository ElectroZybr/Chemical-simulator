#pragma once

#include "Lattice/Engine/physics/IIntegrator.h"

class Verlet final : public IIntegrator {
public:
    static constexpr std::string_view id = "verlet_mt";
    static constexpr std::string_view description = "integrator_velocity_verlet_mt";
    void step(StepContext& stepContext) override { pipeline(stepContext); }

    void pipeline(StepContext& stepContext) const;
    static void predict(AtomStorage& atomStorage, float dt);
    static void correct(AtomStorage& atomStorage, float dt);
};
