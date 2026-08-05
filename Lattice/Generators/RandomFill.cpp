#include "RandomFill.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace Generators {
namespace {

Generators::Bounds clampBoundsToWorld(const Generators::Bounds& bounds, glm::vec3 worldSize, float margin) {
    const glm::vec3 safeMargin = glm::vec3(std::max(margin, 0.0f));
    const glm::vec3 worldMin = glm::min(safeMargin, worldSize * 0.5f);
    const glm::vec3 worldMax = glm::max(worldSize - safeMargin, worldMin);

    return {
        .min = glm::clamp(bounds.min, worldMin, worldMax),
        .max = glm::clamp(bounds.max, worldMin, worldMax),
    };
}

bool containsWithMargin(const Generators::Region& region, glm::vec3 point, float margin) {
    if (!region.contains(point)) {
        return false;
    }

    if (margin <= 0.0f) {
        return true;
    }

    return region.contains(point + glm::vec3( margin, 0.0f, 0.0f)) &&
           region.contains(point + glm::vec3(-margin, 0.0f, 0.0f)) &&
           region.contains(point + glm::vec3(0.0f,  margin, 0.0f)) &&
           region.contains(point + glm::vec3(0.0f, -margin, 0.0f)) &&
           region.contains(point + glm::vec3(0.0f, 0.0f,  margin)) &&
           region.contains(point + glm::vec3(0.0f, 0.0f, -margin));
}

void loadBaseMoleculesOnce(Lattice::Simulation& sim) {
    static bool loaded = false;
    if (loaded) {
        return;
    }

    const std::filesystem::path moleculesPath = std::filesystem::path("Mods") / "Base" / "Molecules";
    if (!std::filesystem::exists(moleculesPath) || !std::filesystem::is_directory(moleculesPath)) {
        loaded = true;
        return;
    }

    std::vector<std::filesystem::path> molFiles;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(moleculesPath)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".mol") {
            continue;
        }
        molFiles.push_back(entry.path());
    }

    std::sort(molFiles.begin(), molFiles.end());
    for (const std::filesystem::path& molPath : molFiles) {
        const std::string moleculeName = molPath.stem().string();
        if (!moleculeName.empty()) {
            sim.loadMoleculeTemplate(moleculeName, molPath);
        }
    }

    loaded = true;
}

} // namespace

int randomFill(Lattice::Simulation& sim, const Generators::Region& region, const std::vector<Compose>& composition,
               const RandomFillOptions& options) {
    if (composition.empty() || options.density <= 0.0f) {
        return 0;
    }

    loadBaseMoleculesOnce(sim);
    const size_t initialAtomCount = sim.atoms().size();

    const float margin = std::max(options.margin, 0.0f);
    const Generators::Bounds bounds = clampBoundsToWorld(region.bounds(), sim.world().getWorldSize(), margin);
    const glm::vec3 span = bounds.max - bounds.min;
    const float boundsVolume = std::max(0.0f, span.x) * std::max(0.0f, span.y) * std::max(0.0f, span.z);
    const int totalAtomBudget = std::max(0, static_cast<int>(std::lround(boundsVolume * options.density)));
    if (totalAtomBudget <= 0) {
        return 0;
    }

    float totalFraction = 0.0f;
    for (const Compose& entry : composition) {
        if (!entry.species.empty() && entry.fraction > 0.0f) {
            totalFraction += entry.fraction;
        }
    }
    if (totalFraction <= 0.0f) {
        return 0;
    }

    std::vector<std::string> speciesQueue;
    size_t totalCount = 0;
    for (const Compose& entry : composition) {
        if (entry.species.empty() || entry.fraction <= 0.0f) {
            continue;
        }

        int atomsPerSpawn = 1;
        if (const MoleculeTemplate* molecule = sim.findMoleculeTemplate(entry.species); molecule != nullptr) {
            atomsPerSpawn = std::max<int>(1, static_cast<int>(molecule->atoms.size()));
        }

        const float normalizedFraction = entry.fraction / totalFraction;
        const int spawnCount = std::max(0, static_cast<int>(std::lround((totalAtomBudget * normalizedFraction) / atomsPerSpawn)));
        totalCount += static_cast<size_t>(spawnCount);
    }
    speciesQueue.reserve(totalCount);

    for (const Compose& entry : composition) {
        if (entry.species.empty() || entry.fraction <= 0.0f) {
            continue;
        }

        int atomsPerSpawn = 1;
        if (const MoleculeTemplate* molecule = sim.findMoleculeTemplate(entry.species); molecule != nullptr) {
            atomsPerSpawn = std::max<int>(1, static_cast<int>(molecule->atoms.size()));
        }

        const float normalizedFraction = entry.fraction / totalFraction;
        const int spawnCount = std::max(0, static_cast<int>(std::lround((totalAtomBudget * normalizedFraction) / atomsPerSpawn)));
        for (int i = 0; i < spawnCount; ++i) {
            speciesQueue.push_back(entry.species);
        }
    }

    if (speciesQueue.empty()) {
        return 0;
    }

    std::mt19937 rng(options.seed == 0 ? std::random_device{}() : options.seed);
    std::shuffle(speciesQueue.begin(), speciesQueue.end(), rng);
    sim.beginAtomBatch();
    Lattice::SpawnOptions spawnOptions;
    spawnOptions.min = bounds.min;
    spawnOptions.max = bounds.max;
    spawnOptions.temperature = options.temperature;
    spawnOptions.margin = margin;
    spawnOptions.maxAttempts = std::max<uint32_t>(1, options.maxAttemptsPerSpawn);
    spawnOptions.randomRotation = options.randomRotation;
    spawnOptions.fixed = options.fixed;
    spawnOptions.collisionMode =
        options.mode == SpawnMode::Replace ? Lattice::SpawnCollisionMode::Replace : Lattice::SpawnCollisionMode::Add;
    spawnOptions.replaceExistingCount = initialAtomCount;

    int spawned = 0;
    for (const std::string& species : speciesQueue) {
        if (sim.randomSpawn(species, spawnOptions)) {
            ++spawned;
        }
    }
    sim.finishAtomBatch();

    const glm::vec3 worldSize = sim.world().getWorldSize();
    std::vector<size_t> atomIndicesToRemove;
    for (size_t atomIndex = sim.atoms().size(); atomIndex > initialAtomCount; --atomIndex) {
        const size_t currentIndex = atomIndex - 1;
        const glm::vec3 position = sim.atoms().pos(currentIndex);
        const bool insideWorld =
            position.x >= margin && position.y >= margin && position.z >= margin &&
            position.x <= worldSize.x - margin &&
            position.y <= worldSize.y - margin &&
            position.z <= worldSize.z - margin;
        if (!insideWorld || !containsWithMargin(region, position, margin)) {
            atomIndicesToRemove.push_back(currentIndex);
        }
    }
    sim.removeAtoms(std::move(atomIndicesToRemove));

    return static_cast<int>(sim.atoms().size() - initialAtomCount);
}

} // namespace Generators
