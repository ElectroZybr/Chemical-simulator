#pragma once

#include "Engine/physics/AtomData.h"
#include "Engine/physics/AtomStorage.h"

namespace EnergyMetrics {
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

    inline float kineticEnergy(AtomData::Type type, const Vec3f& speed) { return 0.5f * AtomData::getProps(type).mass * speed.sqrAbs(); }

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
            totalKineticEnergy += kineticEnergy(atomStorage.type(atomIndex), atomStorage.vel(atomIndex));
            totalSpeed += atomStorage.vel(atomIndex).abs();
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
