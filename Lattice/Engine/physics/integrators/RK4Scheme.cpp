#include "RK4Scheme.h"

#include "Engine/physics/integrators/VerletScheme.h"

void RK4Scheme::pipeline(StepData& stepData) const { VerletScheme{}.pipeline(stepData); }
