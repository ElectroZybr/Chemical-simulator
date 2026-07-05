#include "Langevin.h"

#include "Integrators/Verlet.h"

void Langevin::pipeline(StepContext& stepContext) const {
    Verlet{}.pipeline(stepContext);
}
