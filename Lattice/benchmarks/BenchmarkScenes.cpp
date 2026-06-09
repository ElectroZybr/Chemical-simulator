#include "BenchmarkScenes.h"

#include <algorithm>
#include <cmath>

#include "Engine/Simulation.h"
#include "Lattice/Generators/Generators.h"
using namespace Lattice;

namespace Benchmarks {
    void Scenes::build(Lattice::Simulation& simulation, SceneKind scene, int atomCount) {
        simulation.clear();

        switch (scene) {
        case SceneKind::IdealCrystal3D:
            buildIdealCrystal3D(simulation, atomCount);
            break;
        case SceneKind::Crystal2D:
            buildCrystal2D(simulation, atomCount);
            break;
        case SceneKind::Crystal3D:
            buildCrystal3D(simulation, atomCount);
            break;
        case SceneKind::RandomGas2D:
            buildRandomGas2D(simulation, atomCount);
            break;
        }
    }

    int squareSideFromCount(int atomCount) { return std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(atomCount))))); }

    int cubeSideFromCount(int atomCount) { return std::max(1, static_cast<int>(std::ceil(std::cbrt(static_cast<double>(atomCount))))); }


    void Scenes::buildIdealCrystal3D(Lattice::Simulation& simulation, int atomCount) {
        const int side = cubeSideFromCount(atomCount);
        // glm::ivec3(side) = (side,side,side) — broadcast скаляра по всем осям даёт полный
        // 3D-объём, а не 1D-линию (форк-фикс прежней int-перегрузки теперь покрыт upstream).
        Generators::hexLattice(simulation, glm::ivec3(side), AtomData::Type::Z);
    }

    void Scenes::buildCrystal2D(Lattice::Simulation& simulation, int atomCount) {
        const int side = squareSideFromCount(atomCount);
        Generators::massive(simulation, side, AtomData::Type::Z, false);
    }

    void Scenes::buildCrystal3D(Lattice::Simulation& simulation, int atomCount) {
        const int side = cubeSideFromCount(atomCount);
        Generators::massive(simulation, side, AtomData::Type::Z, true);
    }

    void Scenes::buildRandomGas2D(Lattice::Simulation& simulation, int atomCount) {
        Generators::randomGas(simulation, atomCount, AtomData::Type::H, false);
    }
}
