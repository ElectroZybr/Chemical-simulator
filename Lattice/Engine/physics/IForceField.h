#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>

#include "Lattice/Engine/physics/Bond.h"
#include "Lattice/Engine/ModuleRegistry.hpp"

class World;
class AtomStorage;
class SpatialGrid;
class ChemistryData;

class IForceField : public IModule {
public:
    virtual ~IForceField() = default;
    struct FieldSample {
        float potential = 0.0f;
        glm::vec3 field{0.0f};
    };

    virtual bool compute(World& world, const ChemistryData& chemistryData, bool allowBondFormation, float dt) const = 0;
    virtual FieldSample fieldAtPoint(const AtomStorage& atoms, const SpatialGrid& grid, float x, float y, float z) const {
        (void)atoms;
        (void)grid;
        (void)x;
        (void)y;
        (void)z;
        return {};
    }
};

class ForceField : public RegisteredModuleOwner<ForceField, IForceField> {
public:
    static constexpr bool allowEmpty = true;

    ForceField() { setForceField("classic_md"); }
    ForceField(const ForceField&) = delete;
    ForceField& operator=(const ForceField&) = delete;
    ForceField(ForceField&&) noexcept = default;
    ForceField& operator=(ForceField&&) noexcept = default;

    static ModuleRegistry<IForceField>& registry();

    bool setForceField(std::string_view id) { return set(id); }
    std::string_view getForceField() const { return get(); }
    IForceField* activeForceField() const { return active(); }

    bool compute(World& world, const ChemistryData& chemistryData, bool allowBondFormation, float dt) const {
        return impl_ != nullptr ? impl_->compute(world, chemistryData, allowBondFormation, dt) : false;
    }
    IForceField::FieldSample fieldAtPoint(const AtomStorage& atoms, const SpatialGrid& grid, float x, float y, float z) const {
        return impl_ != nullptr ? impl_->fieldAtPoint(atoms, grid, x, y, z) : IForceField::FieldSample{};
    }
};
