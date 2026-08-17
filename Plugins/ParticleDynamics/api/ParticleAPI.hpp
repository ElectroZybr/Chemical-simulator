#pragma once

#include "Lattice/Engine/physics/Atom/AtomData.h"

namespace ParticleDynamics {
struct IntegratorAPI {
    virtual void step() = 0;
};

struct ForceFieldAPI {
    virtual bool compute() = 0;
};

struct SpatialIndexAPI{
    virtual void rebuild() = 0;
};
}
