#include "LangevinScheme.h"

#include "Engine/physics/integrators/VerletScheme.h"

void LangevinScheme::pipeline(StepData& stepData) const { VerletScheme{}.pipeline(stepData); }
