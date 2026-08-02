#include "MoleculeMol.hpp"
#include "Lattice/Engine/io//ParseOps.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Lattice {
namespace {

MoleculeTemplate loadTemplateImpl(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("MoleculeMol: failed to open file '" + path.string() + "'");
    }

    MoleculeTemplate molecule;

    std::string line;

    // Header (3 строки)
    std::getline(input, molecule.name);
    std::getline(input, line);
    std::getline(input, line);

    // Counts line
    if (!std::getline(input, line)) {
        throw std::runtime_error("MoleculeMol: invalid file");
    }

    std::istringstream counts(line);

    int atomCount = 0;
    int bondCount = 0;

    counts >> atomCount >> bondCount;

    if (atomCount <= 0) {
        throw std::runtime_error("MoleculeMol: no atoms");
    }

    // Atoms
    for (int i = 0; i < atomCount; ++i) {
        if (!std::getline(input, line))
            throw std::runtime_error("MoleculeMol: unexpected EOF");

        std::istringstream stream(line);

        float x, y, z;
        std::string symbol;

        stream >> x >> y >> z >> symbol;

        molecule.atoms.push_back({
            parseAtomTypeFromSymbol(symbol),
            glm::vec3(x, y, z)
        });
    }

    // Bonds
    for (int i = 0; i < bondCount; ++i) {
        if (!std::getline(input, line))
            throw std::runtime_error("MoleculeMol: unexpected EOF");

        std::istringstream stream(line);

        int atomA;
        int atomB;
        int order;

        stream >> atomA >> atomB >> order;

        molecule.bonds.push_back({
            static_cast<uint32_t>(atomA - 1),
            static_cast<uint32_t>(atomB - 1),
            static_cast<uint8_t>(order)
        });
    }

    glm::vec3 centroid(0.0f);

    for (const auto& atom : molecule.atoms)
        centroid += atom.localPos;

    centroid /= static_cast<float>(molecule.atoms.size());

    for (auto& atom : molecule.atoms)
        atom.localPos -= centroid;

    return molecule;
}

} // namespace

MoleculeTemplate MoleculeMol::loadTemplate(const std::filesystem::path& path) {
    return loadTemplateImpl(path);
}

} // namespace Lattice