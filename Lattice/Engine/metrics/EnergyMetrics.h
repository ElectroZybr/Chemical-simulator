#pragma once

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include "Engine/Consts.h"
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"

namespace EnergyMetrics {
    using glm::dot;
    using glm::length;
    using glm::vec3;

    struct Snapshot {
        float averageKineticEnergyEv = 0.0f;
        float averagePotentialEnergyEv = 0.0f;
        float averageSpeedSim = 0.0f;

        float fullAverageEnergyEv() const {
            return averageKineticEnergyEv + averagePotentialEnergyEv;
        }

        float temperatureK() const {
            return 2.0f * averageKineticEnergyEv / (3.0f * Units::kboltzmann);
        }

        float temperatureC() const {
            return temperatureK() - 273.15f;
        }

        float averageSpeedKmPerHour() const {
            return averageSpeedSim * Units::SpeedUnitToKmph;
        }
    };

    inline float kineticEnergy(AtomData::Type type, const vec3& speed) {
        return 0.5f * AtomData::getProps(type).mass * dot(speed, speed);
    }

    inline Snapshot buildSnapshot(const AtomStorage& atomStorage) {
        Snapshot snapshot;
        if (atomStorage.empty()) {
            return snapshot;
        }

        float totalKineticEnergy = 0.0f;
        float totalPotentialEnergy = 0.0f;
        float totalSpeed = 0.0f;
        const size_t mobileCount = atomStorage.mobileCount();
        for (size_t atomIndex = 0; atomIndex < mobileCount; ++atomIndex) {
            const vec3 speed = atomStorage.vel(atomIndex);
            totalKineticEnergy += kineticEnergy(atomStorage.type(atomIndex), speed);
            totalSpeed += length(speed);
        }
        for (size_t atomIndex = 0; atomIndex < atomStorage.size(); ++atomIndex) {
            totalPotentialEnergy += atomStorage.energy(atomIndex);
        }

        const float invAtomCount = 1.0f / atomStorage.size();
        const float invMobileCount = mobileCount > 0 ? 1.0f / mobileCount : 0.0f;
        snapshot.averageKineticEnergyEv = totalKineticEnergy * invMobileCount;
        snapshot.averagePotentialEnergyEv = totalPotentialEnergy * invAtomCount;
        snapshot.averageSpeedSim = totalSpeed * invMobileCount;
        return snapshot;
    }

    inline float averageKineticEnergy(const AtomStorage& atomStorage) {
        return buildSnapshot(atomStorage).averageKineticEnergyEv;
    }

    inline float averagePotentialEnergy(const AtomStorage& atomStorage) {
        return buildSnapshot(atomStorage).averagePotentialEnergyEv;
    }

    inline float fullAverageEnergy(const AtomStorage& atomStorage) {
        return buildSnapshot(atomStorage).fullAverageEnergyEv();
    }

} // namespace EnergyMetrics
