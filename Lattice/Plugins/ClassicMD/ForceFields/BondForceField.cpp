#include "BondForceField.h"

#include <numbers>
#include <vector>

#include "Lattice/Engine/NeighborSearch/NeighborList.h"
#include "Lattice/Engine/metrics/Profiler.h"
#include "Lattice/Engine/physics/BondOps.h"

namespace {
constexpr double kBondBreakDistance = 3.0;
constexpr uint8_t kSingleBondOrder = 0;
}

void BondForceField::breakBond(Bond::List& bonds, Bond* bond, AtomStorage& atomStorage) {
    if (!bond) {
        return;
    }

    detachBond(*bond, atomStorage);

    if (auto it = std::ranges::find_if(bonds, [bond](const Bond& currentBond) { return &currentBond == bond; }); it != bonds.end()) {
        bonds.erase(it);
    }
}

bool BondForceField::compute(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList, bool allowBondFormation, float dt) const {
    PROFILE_SCOPE("ForceField::Bonded");
    if (bonds.empty() && !allowBondFormation) {
        return false;
    }

    // проверка образования и разрыва связей, а также расчет сил
    bool changed = false;
    std::erase_if(bonds, [&](Bond& bond) {
        if (shouldBreak(bond, atoms)) {
            detachBond(bond, atoms);
            changed = true;
            return true;
        }
        return false;
    });

    if (allowBondFormation) {
        changed = formBonds(atoms, bonds, neighborList) || changed;
    }

    for (Bond& bond : bonds) {
        applyBondForce(bond, atoms, dt);
    }

    applyAngleForces(atoms, bonds);
    return changed;
}

bool BondForceField::formBonds(AtomStorage& atoms, Bond::List& bonds, const NeighborList& neighborList) const {
    PROFILE_SCOPE("ForceField::FormBonds(NL)");
    const uint32_t atomCount = static_cast<uint32_t>(atoms.size());
    if (atomCount < 2) {
        return false;
    }

    const auto& offsets = neighborList.offsets();
    const auto& neighbours = neighborList.neighbors();
    bool changed = false;
    for (uint32_t atomIndex = 0; atomIndex < atomCount; ++atomIndex) {
        if (atomIndex + 1 >= offsets.size()) {
            break;
        }
        const uint32_t begin = offsets[atomIndex];
        const uint32_t end = offsets[atomIndex + 1];
        for (uint32_t p = begin; p < end; ++p) {
            changed = tryCreateBond(atoms, bonds, atomIndex, neighbours[p]) || changed;
        }
    }
    return changed;
}

bool BondForceField::tryCreateBond(AtomStorage& atoms, Bond::List& bonds, uint32_t aIndex, uint32_t bIndex) const {
    if (aIndex >= atoms.size() || bIndex >= atoms.size() || aIndex == bIndex) {
        return false;
    }

    const BondParams* bondParams = BondOps::paramsFor(atoms, aIndex, bIndex, kSingleBondOrder);
    if (bondParams == nullptr) {
        return false;
    }

    const float dx = atoms.x()[bIndex] - atoms.x()[aIndex];
    const float dy = atoms.y()[bIndex] - atoms.y()[aIndex];
    const float dz = atoms.z()[bIndex] - atoms.z()[aIndex];
    const float distanceSqr = dx * dx + dy * dy + dz * dz;

    const float formationDistance = std::max(2.5f, bondParams->r0 * 1.35f);
    if (distanceSqr > formationDistance * formationDistance) {
        return false;
    }

    return BondOps::create(bonds, aIndex, bIndex, kSingleBondOrder, atoms) != nullptr;
}

bool BondForceField::shouldBreak(const Bond& bond, const AtomStorage& atoms) {
    if (bond.aIndex >= atoms.size() || bond.bIndex >= atoms.size()) {
        return true;
    }

    const double dx = static_cast<double>(atoms.x()[bond.aIndex]) - atoms.x()[bond.bIndex];
    const double dy = static_cast<double>(atoms.y()[bond.aIndex]) - atoms.y()[bond.bIndex];
    const double dz = static_cast<double>(atoms.z()[bond.aIndex]) - atoms.z()[bond.bIndex];
    const double distanceSqr = dx * dx + dy * dy + dz * dz;
    return distanceSqr > kBondBreakDistance * kBondBreakDistance;
}

void BondForceField::detachBond(const Bond& bond, AtomStorage& atomStorage) {
    if (bond.aIndex < atomStorage.size()) {
        ++atomStorage.valence()[bond.aIndex];
    }
    if (bond.bIndex < atomStorage.size()) {
        ++atomStorage.valence()[bond.bIndex];
    }
}

float BondForceField::morseForce(const Bond& bond, float distance) {
    const float expA = std::exp(-bond.params.a * (distance - bond.params.r0));
    return 2.0f * bond.params.De * bond.params.a * (expA * expA - expA);
}

void BondForceField::applyBondForce(const Bond& bond, AtomStorage& atomStorage, float dt) {
    (void)dt;

    if (bond.aIndex >= atomStorage.size() || bond.bIndex >= atomStorage.size()) {
        return;
    }

    const double dx = static_cast<double>(atomStorage.x()[bond.aIndex]) - atomStorage.x()[bond.bIndex];
    const double dy = static_cast<double>(atomStorage.y()[bond.aIndex]) - atomStorage.y()[bond.bIndex];
    const double dz = static_cast<double>(atomStorage.z()[bond.aIndex]) - atomStorage.z()[bond.bIndex];
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance <= 1e-12) {
        return;
    }

    const double forceMagnitude = morseForce(bond, static_cast<float>(distance));
    const double invDistance = 1.0 / distance;
    const double forceX = dx * invDistance * forceMagnitude;
    const double forceY = dy * invDistance * forceMagnitude;
    const double forceZ = dz * invDistance * forceMagnitude;

    atomStorage.fx()[bond.aIndex] += static_cast<float>(forceX);
    atomStorage.fy()[bond.aIndex] += static_cast<float>(forceY);
    atomStorage.fz()[bond.aIndex] += static_cast<float>(forceZ);

    atomStorage.fx()[bond.bIndex] -= static_cast<float>(forceX);
    atomStorage.fy()[bond.bIndex] -= static_cast<float>(forceY);
    atomStorage.fz()[bond.bIndex] -= static_cast<float>(forceZ);
}

void BondForceField::applyAngleForce(AtomStorage& atomStorage, size_t aIndex, size_t bIndex, size_t cIndex) {
    const double ox = atomStorage.x()[aIndex];
    const double oy = atomStorage.y()[aIndex];
    const double oz = atomStorage.z()[aIndex];
    const double bx = atomStorage.x()[bIndex];
    const double by = atomStorage.y()[bIndex];
    const double bz = atomStorage.z()[bIndex];
    const double cx = atomStorage.x()[cIndex];
    const double cy = atomStorage.y()[cIndex];
    const double cz = atomStorage.z()[cIndex];

    const double delta_ob_x = bx - ox;
    const double delta_ob_y = by - oy;
    const double delta_ob_z = bz - oz;
    const double delta_oc_x = cx - ox;
    const double delta_oc_y = cy - oy;
    const double delta_oc_z = cz - oz;

    const double len_ob = std::sqrt(delta_ob_x * delta_ob_x + delta_ob_y * delta_ob_y + delta_ob_z * delta_ob_z);
    const double len_oc = std::sqrt(delta_oc_x * delta_oc_x + delta_oc_y * delta_oc_y + delta_oc_z * delta_oc_z);
    if (len_ob <= 1e-12 || len_oc <= 1e-12) {
        return;
    }

    const double ob_hat_x = delta_ob_x / len_ob;
    const double ob_hat_y = delta_ob_y / len_ob;
    const double ob_hat_z = delta_ob_z / len_ob;
    const double oc_hat_x = delta_oc_x / len_oc;
    const double oc_hat_y = delta_oc_y / len_oc;
    const double oc_hat_z = delta_oc_z / len_oc;

    double cos_theta = ob_hat_x * oc_hat_x + ob_hat_y * oc_hat_y + ob_hat_z * oc_hat_z;
    cos_theta = std::clamp(cos_theta, -1.0, 1.0);
    double sin_theta_sqr = 1.0 - cos_theta * cos_theta;
    if (sin_theta_sqr < 1e-12) {
        return;
    }

    double angle_theta = std::acos(cos_theta);
    constexpr double theta_0 = 104.5 / 180.0 * std::numbers::pi;
    double angle_loss = angle_theta - theta_0;

    double sin_theta = std::sqrt(sin_theta_sqr);

    constexpr double k = 100;
    const double force_scale = -k * angle_loss / sin_theta;
    const double force_b_x = -((oc_hat_x - ob_hat_x * cos_theta) / len_ob) * force_scale;
    const double force_b_y = -((oc_hat_y - ob_hat_y * cos_theta) / len_ob) * force_scale;
    const double force_b_z = -((oc_hat_z - ob_hat_z * cos_theta) / len_ob) * force_scale;
    const double force_c_x = -((ob_hat_x - oc_hat_x * cos_theta) / len_oc) * force_scale;
    const double force_c_y = -((ob_hat_y - oc_hat_y * cos_theta) / len_oc) * force_scale;
    const double force_c_z = -((ob_hat_z - oc_hat_z * cos_theta) / len_oc) * force_scale;
    const double force_o_x = -(force_b_x + force_c_x);
    const double force_o_y = -(force_b_y + force_c_y);
    const double force_o_z = -(force_b_z + force_c_z);

    atomStorage.fx()[bIndex] += static_cast<float>(force_b_x);
    atomStorage.fy()[bIndex] += static_cast<float>(force_b_y);
    atomStorage.fz()[bIndex] += static_cast<float>(force_b_z);

    atomStorage.fx()[cIndex] += static_cast<float>(force_c_x);
    atomStorage.fy()[cIndex] += static_cast<float>(force_c_y);
    atomStorage.fz()[cIndex] += static_cast<float>(force_c_z);

    atomStorage.fx()[aIndex] += static_cast<float>(force_o_x);
    atomStorage.fy()[aIndex] += static_cast<float>(force_o_y);
    atomStorage.fz()[aIndex] += static_cast<float>(force_o_z);
}

void BondForceField::applyAngleForces(AtomStorage& atoms, const Bond::List& bonds) {
    if (bonds.size() < 2) {
        return;
    }

    std::vector<uint16_t> degree(atoms.size(), 0);
    for (const Bond& bond : bonds) {
        if (bond.aIndex < atoms.size() && bond.bIndex < atoms.size()) {
            ++degree[bond.aIndex];
            ++degree[bond.bIndex];
        }
    }

    std::vector<std::vector<size_t>> bondedNeighbours(atoms.size());
    for (size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
        if (degree[atomIndex] > 0) {
            bondedNeighbours[atomIndex].reserve(degree[atomIndex]);
        }
    }

    for (const Bond& bond : bonds) {
        if (bond.aIndex < atoms.size() && bond.bIndex < atoms.size()) {
            bondedNeighbours[bond.aIndex].emplace_back(bond.bIndex);
            bondedNeighbours[bond.bIndex].emplace_back(bond.aIndex);
        }
    }

    for (size_t atomIndex = 0; atomIndex < bondedNeighbours.size(); ++atomIndex) {
        const auto& neighbours = bondedNeighbours[atomIndex];
        if (neighbours.size() < 2) {
            continue;
        }

        for (size_t i = 0; i + 1 < neighbours.size(); ++i) {
            for (size_t j = i + 1; j < neighbours.size(); ++j) {
                applyAngleForce(atoms, atomIndex, neighbours[i], neighbours[j]);
            }
        }
    }
}
