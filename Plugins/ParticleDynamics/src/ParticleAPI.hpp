#pragma once

namespace Lattice {
class Universe;
}
namespace ParticleDynamics {
    
struct IntegratorAPI {
    virtual void step(float dt) = 0;
};

}