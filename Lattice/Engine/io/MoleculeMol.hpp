#pragma once

#include <filesystem>

#include "Lattice/Engine/io/MoleculeTemplate.h"

namespace Lattice {

class MoleculeMol {
public:
    [[nodiscard]] static MoleculeTemplate loadTemplate(const std::filesystem::path& path);
};

} // namespace Lattice