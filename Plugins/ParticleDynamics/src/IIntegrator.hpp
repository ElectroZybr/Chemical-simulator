#pragma once

namespace ParticleDynamics {

struct StepContext;

class IIntegrator {
public:
    virtual ~IIntegrator() = default;
    virtual void step(StepContext& stepContext) = 0;
};

}