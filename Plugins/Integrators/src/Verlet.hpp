#pragma once

#include <string>

#include <Lattice/Log.hpp>

#include <Plugins/ParticleDynamics/src/ParticleAPI.hpp>
#include <Plugins/ClassicMD/src/AtomStorage.hpp>

#include <Lattice/Kernel/Component.hpp>

namespace Integrators {

class Verlet final : public ParticleDynamics::IntegratorAPI{
public:
    struct PrevForceX {using type = float;};
    struct PrevForceY {using type = float;};
    struct PrevForceZ {using type = float;};

    Verlet(Lattice::Components& components) 
        // интегратор требует для работы буфер. Если нет - исключение
        : atoms(components.require<ClassicMD::AtomStorage>())
    {
        // дополнительные поля для работы интегратора
        atoms->add<PrevForceX>();
        atoms->add<PrevForceY>();
        atoms->add<PrevForceZ>();
    }

    void step(float dt) override { Log::ok("Verlet", "step dt: {}", dt); }

    ~Verlet () {
        atoms->remove<PrevForceX>();
        atoms->remove<PrevForceY>();
        atoms->remove<PrevForceZ>();
    }

private:
    void pipeline() const;
    static void predict();
    static void correct();

    Lattice::Component<ClassicMD::AtomStorage> atoms;
};
}