#pragma once

struct IntegratorAPI {
    static constexpr std::string_view apiName = "Integrator";

    virtual void step(float dt) = 0;
};