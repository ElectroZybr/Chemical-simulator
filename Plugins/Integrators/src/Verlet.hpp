#pragma once

#include <string>

#include "Lattice/Log.hpp"

struct StepContext;
class AtomStorage;

class Verlet {
public:
    static constexpr std::string_view id = "verlet";
    static constexpr std::string_view description = "integrator_velocity_verlet";
    void step(float dt) { Log::ok("Verlet", "step dt: {}", dt); }
private:
    void pipeline(StepContext& stepContext) const;
    static void predict(AtomStorage& atomStorage, float dt);
    static void correct(AtomStorage& atomStorage, float dt);
};
