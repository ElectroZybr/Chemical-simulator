#pragma once

#include "Engine/math/Vec3.h"
#include "Engine/physics/Integrator.h"

namespace Lattice {
    class Simulation;
}

namespace Benchmarks {
    enum class SceneKind {
        IdealCrystal3D,
        Crystal2D,
        Crystal3D,
        RandomGas2D,
    };

    class Scenes {
    public:
        static void build(Lattice::Simulation& simulation, SceneKind scene, int atomCount);
    private:
        static void buildIdealCrystal3D(Lattice::Simulation& simulation, int atomCount);
        static void buildCrystal2D(Lattice::Simulation& simulation, int atomCount);
        static void buildCrystal3D(Lattice::Simulation& simulation, int atomCount);
        static void buildRandomGas2D(Lattice::Simulation& simulation, int atomCount);
    };
}
