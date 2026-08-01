#pragma once

#include "Lattice/Plugins/ClassicMD/Integrators/StepOps.h"
#include "Lattice/Plugins/ClassicMD/Integrators/Verlet.h"

struct ClassicMDStepOpsAdapter {
    static void computeForces(StepContext& stepContext) { StepOps::computeForces(stepContext); }

    template <typename StepFn>
    static void predictAndSync(StepContext& stepContext, StepFn stepFn) {
        StepOps::predictAndSync(stepContext, stepFn);
    }
};