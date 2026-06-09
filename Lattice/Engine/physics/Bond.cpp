#include "Bond.h"

#include <algorithm>
#include <cmath>

#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"

namespace {
    constexpr double kBondBreakDistance = 3.0;
}

BondTable Bond::bond_default_props;

void Bond::ensureInitialized() {
    static const bool initialized = [] {
        Bond::bond_default_props.init();
        return true;
    }();
    (void)initialized;
}

Bond::Bond(size_t aIndex, size_t bIndex, AtomData::Type aType, AtomData::Type bType) : aIndex(aIndex), bIndex(bIndex) {
    const BondParams bondParams = bond_default_props.get(aType, bType);
    params.r0 = bondParams.r0;
    params.a = bondParams.a;
    params.De = bondParams.De;
}

void Bond::forceBond(AtomStorage& atomStorage, float dt) {
    (void)dt;

    if (aIndex >= atomStorage.size() || bIndex >= atomStorage.size()) {
        return;
    }

    const double dx = static_cast<double>(atomStorage.posX(aIndex)) - atomStorage.posX(bIndex);
    const double dy = static_cast<double>(atomStorage.posY(aIndex)) - atomStorage.posY(bIndex);
    const double dz = static_cast<double>(atomStorage.posZ(aIndex)) - atomStorage.posZ(bIndex);
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance <= 1e-12) {
        return;
    }

    const double forceMagnitude = MorseForce(static_cast<float>(distance));
    const double invDistance = 1.0 / distance;
    const double forceX = dx * invDistance * forceMagnitude;
    const double forceY = dy * invDistance * forceMagnitude;
    const double forceZ = dz * invDistance * forceMagnitude;

    atomStorage.forceX(aIndex) += static_cast<float>(forceX);
    atomStorage.forceY(aIndex) += static_cast<float>(forceY);
    atomStorage.forceZ(aIndex) += static_cast<float>(forceZ);

    atomStorage.forceX(bIndex) -= static_cast<float>(forceX);
    atomStorage.forceY(bIndex) -= static_cast<float>(forceY);
    atomStorage.forceZ(bIndex) -= static_cast<float>(forceZ);
}

bool Bond::shouldBreak(const AtomStorage& atomStorage) const {
    if (aIndex >= atomStorage.size() || bIndex >= atomStorage.size()) {
        return true;
    }

    const double dx = static_cast<double>(atomStorage.posX(aIndex)) - atomStorage.posX(bIndex);
    const double dy = static_cast<double>(atomStorage.posY(aIndex)) - atomStorage.posY(bIndex);
    const double dz = static_cast<double>(atomStorage.posZ(aIndex)) - atomStorage.posZ(bIndex);
    const double distanceSqr = dx * dx + dy * dy + dz * dz;
    return distanceSqr > kBondBreakDistance * kBondBreakDistance;
}

float Bond::MorseForce(float distanse) {
    const float expA = std::exp(-params.a * (distanse - params.r0));
    return 2.0f * params.De * params.a * (expA * expA - expA);
}

void Bond::angleForce(AtomStorage& atomStorage, size_t aIndex, size_t bIndex, size_t cIndex) {
    const double ox = atomStorage.posX(aIndex);
    const double oy = atomStorage.posY(aIndex);
    const double oz = atomStorage.posZ(aIndex);
    const double bx = atomStorage.posX(bIndex);
    const double by = atomStorage.posY(bIndex);
    const double bz = atomStorage.posZ(bIndex);
    const double cx = atomStorage.posX(cIndex);
    const double cy = atomStorage.posY(cIndex);
    const double cz = atomStorage.posZ(cIndex);

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

    constexpr double k = 50;
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

    atomStorage.forceX(bIndex) += static_cast<float>(force_b_x);
    atomStorage.forceY(bIndex) += static_cast<float>(force_b_y);
    atomStorage.forceZ(bIndex) += static_cast<float>(force_b_z);

    atomStorage.forceX(cIndex) += static_cast<float>(force_c_x);
    atomStorage.forceY(cIndex) += static_cast<float>(force_c_y);
    atomStorage.forceZ(cIndex) += static_cast<float>(force_c_z);

    atomStorage.forceX(aIndex) += static_cast<float>(force_o_x);
    atomStorage.forceY(aIndex) += static_cast<float>(force_o_y);
    atomStorage.forceZ(aIndex) += static_cast<float>(force_o_z);
}

Bond* Bond::CreateBond(List& bonds, size_t aIndex, size_t bIndex, AtomStorage& atomStorage, Adjacency* adjacency) {
    ensureInitialized();

    if (aIndex >= atomStorage.size() || bIndex >= atomStorage.size() || aIndex == bIndex) {
        return nullptr;
    }

    if (atomStorage.valenceCount(aIndex) <= 0 || atomStorage.valenceCount(bIndex) <= 0) {
        return nullptr;
    }

    if (adjacency != nullptr) {
        // O(degree) lookup в per-atom списке, обычно ≤ valence.
        const auto& neighbors = (*adjacency)[aIndex];
        const auto target = static_cast<uint32_t>(bIndex);
        if (std::find(neighbors.begin(), neighbors.end(), target) != neighbors.end()) {
            return nullptr;
        }
    }
    else if (std::ranges::any_of(bonds, [&](const Bond& bond) {
                 return (bond.aIndex == aIndex && bond.bIndex == bIndex) || (bond.aIndex == bIndex && bond.bIndex == aIndex);
             })) {
        return nullptr;
    }

    const BondParams& bondParams = bond_default_props.get(atomStorage.type(aIndex), atomStorage.type(bIndex));
    if (bondParams.r0 <= 0.0f || bondParams.a <= 0.0f || bondParams.De <= 0.0f) {
        return nullptr;
    }

    bonds.emplace_back(aIndex, bIndex, atomStorage.type(aIndex), atomStorage.type(bIndex));
    --atomStorage.valenceCount(aIndex);
    --atomStorage.valenceCount(bIndex);

    if (adjacency != nullptr) {
        (*adjacency)[aIndex].push_back(static_cast<uint32_t>(bIndex));
        (*adjacency)[bIndex].push_back(static_cast<uint32_t>(aIndex));
    }

    return &bonds.back();
}

void Bond::detach(AtomStorage& atomStorage) {
    if (aIndex < atomStorage.size()) {
        ++atomStorage.valenceCount(aIndex);
    }
    if (bIndex < atomStorage.size()) {
        ++atomStorage.valenceCount(bIndex);
    }
}

void Bond::BreakBond(List& bonds, Bond* bond, AtomStorage& atomStorage) {
    if (!bond) {
        return;
    }

    bond->detach(atomStorage);

    if (auto it = std::ranges::find_if(bonds, [bond](const Bond& currentBond) { return &currentBond == bond; }); it != bonds.end()) {
        bonds.erase(it);
    }
}
