#pragma once

#include <Lattice/Log.hpp>

#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>

class KDK final : public IntegratorAPI {
public:
    static constexpr std::string_view id = "kdk";
    static constexpr std::string_view description = "integrator_kdk";

    void step(float dt) override { Log::ok(id, "step dt: {}", dt); }

    // void pipeline(StepContext& stepContext) const;
    // static void halfKick(AtomStorage& atomStorage, float dt);
    // static void drift(AtomStorage& atomStorage, float dt);
};
