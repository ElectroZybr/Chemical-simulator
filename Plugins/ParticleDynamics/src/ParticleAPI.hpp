#pragma once

#include "Lattice/Engine/physics/Atom/AtomData.h"

namespace ParticleDynamics {
struct IntegratorAPI {
    virtual void step() = 0;
};

}
