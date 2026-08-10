#pragma once

#include "Lattice/Log.hpp"

class Physics {
public:
    static constexpr std::string_view apiName = "Physics";

    void simulate(float dt) {
        Log::ok(apiName, "step with dt: {}", dt);
    }
};

Physics physics;