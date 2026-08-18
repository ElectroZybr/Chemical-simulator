#include "MoleculeTemplatesIO.h"

#include "Lattice/Engine/Simulation.h"
#include <Lattice/Tools/Logger.hpp>

namespace MoleculeTemplatesIO {
void loadFromDirectory(Lattice::Simulation& simulation, const std::filesystem::path& directory) {
    size_t loadedCount = 0;

    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        Logger::warning("Application", "Molecule templates directory is missing: {}", directory.string());
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".mol") {
            continue;
        }

        const std::string moleculeName = entry.path().stem().string();
        if (moleculeName.empty()) {
            continue;
        }

        try {
            simulation.loadMoleculeTemplate(moleculeName, entry.path());
            ++loadedCount;
        }
        catch (const std::exception& e) {
            Logger::warning("Application", "Failed to load molecule template '{}': {}", entry.path().string(), e.what());
        }
    }

    Logger::info("Application", "Loaded {} molecule template(s)", loadedCount);
}
}
