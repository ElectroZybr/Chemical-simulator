#pragma once

#include <Lattice/Log.hpp>

#include <Plugins/ParticleDynamics/src/ParticleAPI.hpp>

namespace Integrators {

class KDK final : public ParticleDynamics::IntegratorAPI {
public:
    KDK(Lattice::Components& components) {
    }

    void step(float dt) override { Log::ok("KDK", "step dt: {}", dt); }

    // void pipeline(StepContext& stepContext) const;
    // static void halfKick(AtomStorage& atomStorage, float dt);
    // static void drift(AtomStorage& atomStorage, float dt);
};

}