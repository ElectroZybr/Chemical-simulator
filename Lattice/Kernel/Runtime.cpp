#include "Runtime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Lattice/Engine/Consts.h"
#include "Lattice/Log.hpp"

namespace Lattice {
bool Runtime::loadPlugins(std::filesystem::path path) {
    // регистрация интерфейсов ядра
    globalRegistry.registerAPI<UniverseModelAPI>();
    // загрузка внешних плагинов
    pluginManager.scanDirectory(path);
    pluginManager.checkCandidates();
    pluginManager.loadCandidates(globalRegistry);
    return true;
}

Universe& Runtime::createUniverse() {
    universes.emplace_back(globalRegistry);
    Universe& universe = universes.back();
    universe.use<UniverseModelAPI>("ClassicMD");
    if (universes.size() == 1) {
        activeUniverseId = universes.size() - 1;
    }
    return universe;
}

bool Runtime::removeUniverse(id universeId) {
    if (universeId >= universes.size() || universes.size() <= 1) {
        Log::warning(moduleName,"Attempted to remove non-existent Universe {}", universeId);
        return false;
    }

    universes.erase(universes.begin() + static_cast<std::ptrdiff_t>(universeId));
    if (activeUniverseId == universeId) {
        activeUniverseId = std::min(universeId, universes.size() - 1);
    }
    else if (activeUniverseId > universeId) {
        --activeUniverseId;
    }
    return true;
}

bool Runtime::setActiveUniverse(id universeId) {
    if (universeId >= universes.size()) {
        Log::warning(moduleName,"Attempted to activate non-existent Universe {}", universeId);
        return false;
    }
    activeUniverseId = universeId;
    return true;
}

Universe& Runtime::universeAt(id universeId) {
    if (universeId >= universes.size()) {
        throw std::out_of_range("Runtime::universeAt: invalid Universe id");
    }
    return universes[universeId];
}

const Universe& Runtime::universeAt(id universeId) const {
    if (universeId >= universes.size()) {
        throw std::out_of_range("Runtime::universeAt: invalid Universe id");
    }
    return universes[universeId];
}

Universe& Runtime::universe() {
    if (universes.empty() || activeUniverseId >= universes.size()) {
        throw std::runtime_error("Runtime: no active Universe");
    }
    return universes[activeUniverseId];
}

const Universe& Runtime::universe() const {
    if (universes.empty() || activeUniverseId >= universes.size()) {
        throw std::runtime_error("Runtime: no active Universe");
    }
    return universes[activeUniverseId];
}
}
