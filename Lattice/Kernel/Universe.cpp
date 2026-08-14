#include <Lattice/Kernel/Universe.hpp>
#include <Lattice/Log.hpp>

namespace Lattice {
Universe::Universe(ModuleRegistry& registry)
        : apis(&registry)
        , model(nullptr) {}

void Universe::configure(std::string_view modelName) {
    apis.require<UniverseModelAPI>();
    apis.use<UniverseModelAPI>(modelName);
    model = apis.get<UniverseModelAPI>();
    if (!model)
        throw std::runtime_error("No UniverseModelAPI selected");

    model->configure(*this);
}

void Universe::update() {
    model->update();
}
}