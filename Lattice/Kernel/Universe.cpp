#include <Lattice/Kernel/Universe.hpp>
#include <Lattice/Log.hpp>
#include <stdexcept>

namespace Lattice {
Universe::Universe(ModuleRegistry& registry)
    : apis(&registry)
    , model(apis.require<UniverseModelAPI>())
{}

void Universe::configure(std::string_view modelName) {
    apis.use<UniverseModelAPI>(modelName);

    if (!model)
        throw std::runtime_error("No UniverseModelAPI selected");

    model->configure(*this);
}

void Universe::update() {
    model->update();
}
}