#include "ScriptAPI.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <random>
#include <stdexcept>

#include <sol/sol.hpp>

namespace Lattice {

namespace {

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string normalizeAtomSymbol(std::string value) {
    value = lowercase(std::move(value));
    if (!value.empty()) {
        value[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[0])));
    }
    return value;
}

std::optional<AtomData::Type> findAtomTypeBySymbol(std::string_view speciesName) {
    const std::string atomSymbol = normalizeAtomSymbol(std::string(speciesName));
    for (size_t i = 0; i < static_cast<size_t>(AtomData::Type::COUNT); ++i) {
        const AtomData::Type type = static_cast<AtomData::Type>(i);
        if (AtomData::symbol(type) == atomSymbol) {
            return type;
        }
    }
    return std::nullopt;
}

} // namespace

glm::vec3 readVec3Object(const sol::object& vecObject, const glm::vec3& fallback = glm::vec3(0.0f)) {
    if (!vecObject.is<sol::table>()) {
        return fallback;
    }
    const sol::table vecTable = vecObject.as<sol::table>();
    return glm::vec3(
        vecTable.get_or("x", vecTable.get_or(1, fallback.x)),
        vecTable.get_or("y", vecTable.get_or(2, fallback.y)),
        vecTable.get_or("z", vecTable.get_or(3, fallback.z))
    );
}

std::string requireStringField(const sol::table& table, const char* key, const char* context) {
    const sol::object object = table[key];
    if (!object.is<std::string>()) {
        throw std::runtime_error(std::string(context) + " requires string field '" + key + "'");
    }
    return object.as<std::string>();
}

bool ScriptBatch::spawn(const std::string& speciesName, const sol::object& positionObject, const sol::object& optionsObject) {
    const glm::vec3 position = readVec3Object(positionObject);
    const SpawnOptions options = parseSpawnOptions(optionsObject);
    if (const std::optional<AtomData::Type> atomType = findAtomTypeBySymbol(speciesName); atomType.has_value()) {
        (void)simulation_.appendAtomFast(position, options.velocity, *atomType, options.fixed);
        return true;
    }
    return simulation_.spawnMolecule(speciesName, position, std::nullopt, options.fixed);
}

bool ScriptBatch::random_spawn(const std::string& speciesName, const sol::object& optionsObject) {
    return simulation_.randomSpawn(speciesName, parseSpawnOptions(optionsObject));
}

void ScriptBatch::finish() {
    if (finished_) {
        return;
    }
    simulation_.finishAtomBatch();
    finished_ = true;
}

SpawnOptions ScriptBatch::parseSpawnOptions(const sol::object& object) const {
    const glm::vec3 worldSize = simulation_.world().getWorldSize();
    SpawnOptions options;
    options.max = worldSize;
    if (!object.is<sol::table>()) {
        return options;
    }

    const sol::table table = object.as<sol::table>();
    if (const sol::object velocity = table["velocity"]; velocity.is<sol::table>()) {
        options.velocity = readVec3Object(velocity);
    }
    if (const sol::object regionObject = table["region"]; regionObject.is<sol::table>()) {
        const sol::table regionTable = regionObject.as<sol::table>();
        options.min = readVec3Object(regionTable["min"], options.min);
        options.max = readVec3Object(regionTable["max"], options.max);
    }
    options.temperature = table.get_or("temperature", options.temperature);
    options.margin = table.get_or("margin", options.margin);
    options.minDistance = table.get_or("min_distance", options.minDistance);
    options.maxAttempts = table.get_or("attempts", options.maxAttempts);
    options.randomRotation = table.get_or("random_rotation", options.randomRotation);
    options.fixed = table.get_or("fixed", options.fixed);
    return options;
}

void ScriptAPI::clear() { simulation_.clear(); }

void ScriptAPI::set_world_title(const std::string& title) { simulation_.setWorldTitle(title); }

void ScriptAPI::set_box(float x, float y, sol::variadic_args args) {
    const float z = args.size() >= 1 ? static_cast<float>(args.get<float>(0)) : 6.0f;
    simulation_.setSizeBox(glm::vec3(x, y, z));
}

std::tuple<float, float, float> ScriptAPI::world_size() const {
    const glm::vec3 size = simulation_.world().getWorldSize();
    return {size.x, size.y, size.z};
}

std::tuple<int, std::vector<std::string>> ScriptAPI::load_molecules(const std::string& path) {
    std::vector<std::string> loadedNames = loadMoleculesFrom(path);
    return {static_cast<int>(loadedNames.size()), std::move(loadedNames)};
}

std::shared_ptr<ScriptBatch> ScriptAPI::begin_batch() {
    simulation_.beginAtomBatch();
    return std::make_shared<ScriptBatch>(simulation_);
}

float ScriptAPI::lj_min(const std::string& speciesA, const std::string& speciesB) const {
    const std::optional<AtomData::Type> atomA = findAtomTypeBySymbol(speciesA);
    const std::optional<AtomData::Type> atomB = findAtomTypeBySymbol(speciesB);
    if (!atomA.has_value() || !atomB.has_value()) {
        throw std::runtime_error("scene:lj_min supports atoms only");
    }
    return simulation_.lj_min(*atomA, *atomB);
}

int ScriptAPI::random_fill(const sol::object& regionObject, const sol::object& compositionObject, const sol::object& optionsObject) {
    const std::unique_ptr<Generators::Region> region = parseRegion(regionObject);
    return ::Generators::randomFill(simulation_, *region, parseComposition(compositionObject, false), parseRandomFillOptions(optionsObject));
}

int ScriptAPI::lattice_fill(const sol::object& regionObject, const sol::object& compositionObject, const sol::object& optionsObject) {
    const std::unique_ptr<Generators::Region> region = parseRegion(regionObject);
    return ::Generators::latticeFill(simulation_, *region, parseComposition(compositionObject, true), parseLatticeFillOptions(optionsObject));
}

std::vector<::Generators::Compose> ScriptAPI::parseComposition(const sol::object& object, bool requireFraction) const {
    if (!object.is<sol::table>()) {
        throw std::runtime_error("composition must be a table");
    }

    const sol::table table = object.as<sol::table>();
    std::vector<::Generators::Compose> composition;
    for (const auto& entry : table) {
        const sol::object value = entry.second.as<sol::object>();
        if (!value.is<sol::table>()) {
            throw std::runtime_error("composition entries must be tables");
        }

        const sol::table item = value.as<sol::table>();
        ::Generators::Compose compose;
        compose.species = item.get_or("name", item.get_or("element", std::string{}));
        if (compose.species.empty()) {
            throw std::runtime_error("composition entry requires name or element");
        }
        compose.fraction = requireFraction ? item.get_or("fraction", 0.0f) : item.get_or("fraction", 1.0f);
        if (compose.fraction <= 0.0f) {
            throw std::runtime_error("composition fraction must be > 0");
        }
        composition.push_back(std::move(compose));
    }

    if (composition.empty()) {
        throw std::runtime_error("composition must not be empty");
    }
    return composition;
}

std::unique_ptr<Generators::Region> ScriptAPI::parseRegion(const sol::object& object) const {
    if (!object.is<sol::table>()) {
        throw std::runtime_error("region must be a table");
    }

    const sol::table table = object.as<sol::table>();
    const std::string kind = requireStringField(table, "kind", "region");

    if (kind == "box") {
        auto region = std::make_unique<Generators::Rectangle>();
        const glm::vec3 min = readVec3Object(table["min"]);
        const glm::vec3 size = readVec3Object(table["size"]);
        region->boxSize = size;
        region->center = min + size * 0.5f;
        return region;
    }

    if (kind == "sphere") {
        auto region = std::make_unique<Generators::Sphere>();
        region->center = readVec3Object(table["center"]);
        region->sphereRadius = table.get_or("radius", 0.0f);
        return region;
    }

    throw std::runtime_error("unsupported region kind: " + kind);
}

::Generators::RandomFillOptions ScriptAPI::parseRandomFillOptions(const sol::object& object) const {
    ::Generators::RandomFillOptions options;
    if (!object.is<sol::table>()) {
        return options;
    }

    const sol::table table = object.as<sol::table>();
    options.density = table.get_or("density", options.density);
    options.temperature = table.get_or("temperature", options.temperature);
    options.margin = table.get_or("margin", options.margin);
    options.maxAttemptsPerSpawn = table.get_or("attempts", options.maxAttemptsPerSpawn);
    options.randomRotation = table.get_or("random_rotation", options.randomRotation);
    options.fixed = table.get_or("fixed", options.fixed);
    options.seed = table.get_or("seed", options.seed);

    const std::string mode = table.get_or("mode", std::string{});
    if (mode == "reset") {
        options.mode = ::Generators::SpawnMode::Reset;
    } else if (mode == "add") {
        options.mode = ::Generators::SpawnMode::Add;
    } else if (mode == "replace" || mode.empty()) {
        options.mode = ::Generators::SpawnMode::Replace;
    } else {
        throw std::runtime_error("unsupported random_fill mode: " + mode);
    }

    return options;
}

::Generators::LatticeFillOptions ScriptAPI::parseLatticeFillOptions(const sol::object& object) const {
    ::Generators::LatticeFillOptions options;
    if (!object.is<sol::table>()) {
        return options;
    }

    const sol::table table = object.as<sol::table>();
    options.margin = table.get_or("margin", options.margin);
    options.fixed = table.get_or("fixed", options.fixed);
    options.seed = table.get_or("seed", options.seed);

    const std::string structure = table.get_or("structure", std::string{"hex"});
    if (structure == "bcc") {
        options.structure = ::Generators::LatticeStructure::Bcc;
    } else if (structure == "hex") {
        options.structure = ::Generators::LatticeStructure::Hex;
    } else {
        throw std::runtime_error("unsupported lattice_fill structure: " + structure);
    }

    const std::string mode = table.get_or("mode", std::string{});
    if (mode == "reset") {
        options.mode = ::Generators::SpawnMode::Reset;
    } else if (mode == "add") {
        options.mode = ::Generators::SpawnMode::Add;
    } else if (mode == "replace" || mode.empty()) {
        options.mode = ::Generators::SpawnMode::Replace;
    } else {
        throw std::runtime_error("unsupported lattice_fill mode: " + mode);
    }

    return options;
}

std::vector<std::string> ScriptAPI::loadMoleculesFrom(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("molecule path does not exist: '" + path.string() + "'");
    }

    std::vector<std::filesystem::path> molFiles;
    if (std::filesystem::is_regular_file(path)) {
        if (lowercase(path.extension().string()) != ".mol") {
            throw std::runtime_error("molecule file must have .mol extension: '" + path.string() + "'");
        }
        molFiles.push_back(path);
    } else if (std::filesystem::is_directory(path)) {
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (lowercase(entry.path().extension().string()) != ".mol") {
                continue;
            }
            molFiles.push_back(entry.path());
        }
    } else {
        throw std::runtime_error("molecule path must be a .mol file or directory: '" + path.string() + "'");
    }

    std::sort(molFiles.begin(), molFiles.end());

    std::vector<std::string> loadedNames;
    loadedNames.reserve(molFiles.size());
    for (const std::filesystem::path& molPath : molFiles) {
        const std::string moleculeName = molPath.stem().string();
        if (moleculeName.empty()) {
            continue;
        }

        simulation_.loadMoleculeTemplate(moleculeName, molPath);
        loadedNames.push_back(moleculeName);
    }

    return loadedNames;
}

} // namespace Lattice
