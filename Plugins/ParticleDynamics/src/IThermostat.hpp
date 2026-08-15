#pragma once

namespace ParticleDynamics {

struct StepContext;

class IThermostat {
public:
    virtual ~IThermostat() = default;
    virtual void setTemperature(float temperature) { (void)temperature; }
    virtual float temperature() const { return 0.0f; }
    virtual void apply(StepContext& stepContext) = 0;
};

}