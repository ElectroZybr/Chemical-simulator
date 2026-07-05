#include "RK4.h"

#include "Integrators/Verlet.h"

void RK4::pipeline(StepContext& stepContext) const {
    Verlet{}.pipeline(stepContext);
}
