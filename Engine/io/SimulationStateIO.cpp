#include "SimulationStateIO.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Engine/Simulation.h"

namespace {
    constexpr const char* kBlockIndent = "  ";

    struct LoadedAtomData {
        Vec3f coords{0.f, 0.f, 0.f};
        Vec3f speed{0.f, 0.f, 0.f};
        int type = 0;
        bool fixed = false;
        float charge = 0.0f;
    };

    std::string trim(std::string_view value) {
        size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
            ++begin;
        }

        size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }

        return std::string(value.substr(begin, end - begin));
    }

    std::string readFirstNonEmptyLine(std::ifstream& file) {
        std::string line;
        while (std::getline(file, line)) {
            const std::string trimmed = trim(line);
            if (!trimmed.empty()) {
                return trimmed;
            }
        }
        return {};
    }

    bool isNewLatFormat(const std::string& firstNonEmptyLine) {
        return firstNonEmptyLine == "format lat" || firstNonEmptyLine == "[meta]" ||
               (!firstNonEmptyLine.empty() && firstNonEmptyLine.front() == '[');
    }

    void saveNewFormat(const Simulation& simulation, std::string_view path) {
        std::ofstream file(path.data(), std::ios::trunc);
        if (!file.is_open()) {
            return;
        }

        file << "[meta]\n";
        file << kBlockIndent << "format lat\n";
        file << kBlockIndent << "version 1\n\n";
        file << kBlockIndent << "title " << simulation.worldTitle() << "\n";
        file << kBlockIndent << "description " << simulation.worldDescription() << "\n\n";

        file << "[scene]\n";
        file << kBlockIndent << "box " << simulation.world().getWorldSize().x << " " << simulation.world().getWorldSize().y << " "
             << simulation.world().getWorldSize().z << "\n";
        file << kBlockIndent << "step " << simulation.getSimStep() << "\n";
        file << kBlockIndent << "time_ns " << simulation.simTimeNs() << "\n";
        file << kBlockIndent << "dt " << simulation.getDt() << "\n";
        file << kBlockIndent << "integrator " << static_cast<int>(simulation.getIntegrator()) << "\n";
        const Vec3f gravity = simulation.getGravity();
        file << kBlockIndent << "gravity " << gravity.x << " " << gravity.y << " " << gravity.z << "\n";
        file << kBlockIndent << "bond_formation " << static_cast<int>(simulation.isBondFormationEnabled()) << "\n";
        file << kBlockIndent << "lj_enabled " << static_cast<int>(simulation.isLJEnabled()) << "\n";
        file << kBlockIndent << "coulomb_enabled " << static_cast<int>(simulation.isCoulombEnabled()) << "\n";
        file << kBlockIndent << "cell_size " << simulation.world().getGridCellSize() << "\n";
        file << kBlockIndent << "cutoff_nl " << simulation.getNeighborListCutoff() << "\n";
        file << kBlockIndent << "skin_nl " << simulation.getNeighborListSkin() << "\n";
        file << kBlockIndent << "max_speed " << simulation.getMaxParticleSpeed() << "\n";
        file << kBlockIndent << "accel_damping " << simulation.getAccelDamping() << "\n\n";

        const AtomStorage& atoms = simulation.atoms();
        file << "[atoms]\n";
        file << kBlockIndent << "count " << atoms.size() << "\n";
        file << kBlockIndent << "# atom x y z vx vy vz type fixed charge\n";
        for (size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
            const Vec3f pos = atoms.pos(atomIndex);
            const Vec3f vel = atoms.vel(atomIndex);
            file << kBlockIndent << "atom " << pos.x << " " << pos.y << " " << pos.z << " " << vel.x << " " << vel.y << " " << vel.z << " "
                 << static_cast<int>(atoms.type(atomIndex)) << " " << static_cast<int>(atoms.isAtomFixed(atomIndex)) << " "
                 << atoms.charge(atomIndex) << "\n";
        }
        file << "\n";

        file << "[bonds]\n";
        file << kBlockIndent << "count " << simulation.bonds().size() << "\n";
        for (const Bond& bond : simulation.bonds()) {
            file << kBlockIndent << "bond " << bond.aIndex << " " << bond.bIndex << "\n";
        }
    }

    void loadLegacyFormat(Simulation& simulation, std::string_view path) {
        std::ifstream file(path.data());
        if (!file.is_open()) {
            return;
        }

        simulation.clear();

        std::vector<LoadedAtomData> atoms;
        Vec3f boxSize{simulation.world().getWorldSize()};
        int cellSize = simulation.world().getGridCellSize();
        int loadedStep = 0;
        float loadedTimeNs = 0.0f;
        std::string loadedTitle;
        std::string loadedDescription;
        float loadedDt = simulation.getDt();
        int loadedIntegrator = static_cast<int>(simulation.getIntegrator());
        Vec3f loadedGravity = simulation.getGravity();
        bool loadedBondFormationEnabled = simulation.isBondFormationEnabled();
        float loadedMaxSpeed = simulation.getMaxParticleSpeed();
        float loadedAccelDamping = simulation.getAccelDamping();

        std::string tag;
        while (file >> tag) {
            if (tag == "box") {
                file >> boxSize.x >> boxSize.y >> boxSize.z;
            }
            else if (tag == "title") {
                std::string value;
                std::getline(file, value);
                loadedTitle = trim(value);
            }
            else if (tag == "description") {
                std::string value;
                std::getline(file, value);
                loadedDescription = trim(value);
            }
            else if (tag == "step") {
                file >> loadedStep;
            }
            else if (tag == "time_ns") {
                file >> loadedTimeNs;
            }
            else if (tag == "dt") {
                file >> loadedDt;
            }
            else if (tag == "integrator") {
                file >> loadedIntegrator;
            }
            else if (tag == "gravity") {
                file >> loadedGravity.x >> loadedGravity.y >> loadedGravity.z;
            }
            else if (tag == "bond_formation") {
                int enabled = 0;
                file >> enabled;
                loadedBondFormationEnabled = (enabled != 0);
            }
            else if (tag == "cell_size") {
                file >> cellSize;
            }
            else if (tag == "cutoff_nl") {
                float cutoff = simulation.getNeighborListCutoff();
                file >> cutoff;
                simulation.setNeighborListCutoff(cutoff);
            }
            else if (tag == "skin_nl") {
                float skin = simulation.getNeighborListSkin();
                file >> skin;
                simulation.setNeighborListSkin(skin);
            }
            else if (tag == "max_speed") {
                file >> loadedMaxSpeed;
            }
            else if (tag == "accel_damping") {
                file >> loadedAccelDamping;
            }
            else if (tag == "atom") {
                LoadedAtomData atom;
                int fixed = 0;
                if (!(file >> atom.coords.x >> atom.coords.y >> atom.coords.z >> atom.speed.x >> atom.speed.y >> atom.speed.z >>
                      atom.type >> fixed)) {
                    break;
                }
                atom.fixed = (fixed != 0);
                atom.charge = 0.0f;
                atoms.emplace_back(atom);
            }
            else {
                std::string ignoredLine;
                std::getline(file, ignoredLine);
            }
        }

        simulation.setSizeBox(boxSize, cellSize);
        simulation.setDt(loadedDt);
        simulation.setIntegrator(static_cast<Integrator::Scheme>(loadedIntegrator));
        simulation.setGravity(loadedGravity);
        simulation.setBondFormationEnabled(loadedBondFormationEnabled);
        simulation.setMaxParticleSpeed(loadedMaxSpeed);
        simulation.setAccelDamping(loadedAccelDamping);
        simulation.setWorldTitle(loadedTitle);
        simulation.setWorldDescription(loadedDescription);

        simulation.reserveAtoms(atoms.size());
        for (const LoadedAtomData& atom : atoms) {
            simulation.appendAtomFast(atom.coords, atom.speed, static_cast<AtomData::Type>(atom.type), atom.fixed);
        }
        simulation.finalizeAtomBatch();
        simulation.restoreRuntimeState(loadedStep, loadedTimeNs);
    }

    void loadNewFormat(Simulation& simulation, std::string_view path) {
        std::ifstream file(path.data());
        if (!file.is_open()) {
            return;
        }

        simulation.clear();

        Vec3f worldSize{simulation.world().getWorldSize()};
        int cellSize = simulation.world().getGridCellSize();
        int loadedStep = 0;
        float loadedTimeNs = 0.0f;
        std::string loadedTitle;
        std::string loadedDescription;
        float loadedDt = simulation.getDt();
        int loadedIntegrator = static_cast<int>(simulation.getIntegrator());
        Vec3f loadedGravity = simulation.getGravity();
        bool loadedBondFormationEnabled = simulation.isBondFormationEnabled();
        bool loadedLJEnabled = simulation.isLJEnabled();
        bool loadedCoulombEnabled = simulation.isCoulombEnabled();
        float loadedMaxSpeed = simulation.getMaxParticleSpeed();
        float loadedAccelDamping = simulation.getAccelDamping();

        std::vector<LoadedAtomData> atoms;
        std::vector<std::pair<size_t, size_t>> bonds;

        std::string line;
        while (std::getline(file, line)) {
            const std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed.front() == '#') {
                continue;
            }
            if (trimmed == "format lat" || trimmed == "version 1") {
                continue;
            }
            if (trimmed.front() == '[') {
                continue;
            }

            std::istringstream stream(trimmed);
            std::string tag;
            stream >> tag;

            if (tag == "box") {
                stream >> worldSize.x >> worldSize.y >> worldSize.z;
            }
            else if (tag == "title") {
                std::string value;
                std::getline(stream, value);
                loadedTitle = trim(value);
            }
            else if (tag == "description") {
                std::string value;
                std::getline(stream, value);
                loadedDescription = trim(value);
            }
            else if (tag == "step") {
                stream >> loadedStep;
            }
            else if (tag == "time_ns") {
                stream >> loadedTimeNs;
            }
            else if (tag == "dt") {
                stream >> loadedDt;
            }
            else if (tag == "integrator") {
                stream >> loadedIntegrator;
            }
            else if (tag == "gravity") {
                stream >> loadedGravity.x >> loadedGravity.y >> loadedGravity.z;
            }
            else if (tag == "bond_formation") {
                int enabled = 0;
                stream >> enabled;
                loadedBondFormationEnabled = (enabled != 0);
            }
            else if (tag == "lj_enabled") {
                int enabled = 0;
                stream >> enabled;
                loadedLJEnabled = (enabled != 0);
            }
            else if (tag == "coulomb_enabled") {
                int enabled = 0;
                stream >> enabled;
                loadedCoulombEnabled = (enabled != 0);
            }
            else if (tag == "cell_size") {
                stream >> cellSize;
            }
            else if (tag == "cutoff_nl") {
                float cutoff = simulation.getNeighborListCutoff();
                stream >> cutoff;
                simulation.setNeighborListCutoff(cutoff);
            }
            else if (tag == "skin_nl") {
                float skin = simulation.getNeighborListSkin();
                stream >> skin;
                simulation.setNeighborListSkin(skin);
            }
            else if (tag == "max_speed") {
                stream >> loadedMaxSpeed;
            }
            else if (tag == "accel_damping") {
                stream >> loadedAccelDamping;
            }
            else if (tag == "atom") {
                LoadedAtomData atom;
                int fixed = 0;
                if (!(stream >> atom.coords.x >> atom.coords.y >> atom.coords.z >> atom.speed.x >> atom.speed.y >> atom.speed.z >>
                      atom.type >> fixed)) {
                    continue;
                }
                atom.fixed = (fixed != 0);
                if (!(stream >> atom.charge)) {
                    atom.charge = 0.0f;
                }
                atoms.emplace_back(atom);
            }
            else if (tag == "bond") {
                size_t aIndex = 0;
                size_t bIndex = 0;
                if (stream >> aIndex >> bIndex) {
                    bonds.emplace_back(aIndex, bIndex);
                }
            }
        }

        simulation.setSizeBox(worldSize, cellSize);
        simulation.setDt(loadedDt);
        simulation.setIntegrator(static_cast<Integrator::Scheme>(loadedIntegrator));
        simulation.setGravity(loadedGravity);
        simulation.setBondFormationEnabled(loadedBondFormationEnabled);
        simulation.setLJEnabled(loadedLJEnabled);
        simulation.setCoulombEnabled(loadedCoulombEnabled);
        simulation.setMaxParticleSpeed(loadedMaxSpeed);
        simulation.setAccelDamping(loadedAccelDamping);
        simulation.setWorldTitle(loadedTitle);
        simulation.setWorldDescription(loadedDescription);

        simulation.reserveAtoms(atoms.size());
        for (const LoadedAtomData& atom : atoms) {
            simulation.appendAtomFast(atom.coords, atom.speed, static_cast<AtomData::Type>(atom.type), atom.fixed);
        }
        simulation.finalizeAtomBatch();
        for (size_t i = 0; i < atoms.size(); ++i) {
            simulation.atoms().charge(i) = atoms[i].charge;
        }
        for (const auto& [aIndex, bIndex] : bonds) {
            simulation.addBond(aIndex, bIndex);
        }

        simulation.restoreRuntimeState(loadedStep, loadedTimeNs);
    }
}

void SimulationStateIO::save(const Simulation& simulation, std::string_view path) { saveNewFormat(simulation, path); }

void SimulationStateIO::load(Simulation& simulation, std::string_view path) {
    std::ifstream probe(path.data());
    if (!probe.is_open()) {
        return;
    }

    const std::string firstLine = readFirstNonEmptyLine(probe);
    probe.close();

    if (isNewLatFormat(firstLine)) {
        loadNewFormat(simulation, path);
        return;
    }

    loadLegacyFormat(simulation, path);
}
