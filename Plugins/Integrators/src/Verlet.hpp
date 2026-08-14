#pragma once

#include <string>

#include <Lattice/Log.hpp>

#include <Plugins/LatticeParticleAPI/ParticleAPI.hpp>

struct StepContext;
class AtomStorage;

class Verlet final : public IntegratorAPI{
public:
    static constexpr std::string_view id = "Verlet";
    static constexpr std::string_view description = "integrator_velocity_verlet";
    void step(float dt) override { Log::ok(id, "step dt: {}", dt); }
private:
    void pipeline(StepContext& stepContext) const;
    static void predict(AtomStorage& atomStorage, float dt);
    static void correct(AtomStorage& atomStorage, float dt);
};
