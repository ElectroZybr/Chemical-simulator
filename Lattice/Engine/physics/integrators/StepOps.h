#pragma once

#include <cmath>

#include "Engine/World.h"
#include "Engine/metrics/Profiler.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/ForceField.h"
#include "Engine/physics/Integrator.h"
#include "Engine/restrict.h"

class NeighborList;

namespace StepOps {
    template <typename T>
    concept AtomStepFunc = requires(T fn, AtomStorage& storage, float dt) {
        { fn(storage, dt) } -> std::same_as<void>;
    };

    inline void confineToBox(World& world) {
        constexpr float restitution = 0.8f;
        const glm::vec3 max = world.getWorldSize();

        AtomStorage& atomStorage = world.getAtomStorage();

        auto confineAxis = [&](float& coord, float& speed, float axisMax) {
            if (coord < 0.0f) {
                coord = 0.0f;
                if (speed < 0.0f) {
                    speed = -speed * restitution;
                }
            }
            else if (coord > axisMax) {
                coord = axisMax;
                if (speed > 0.0f) {
                    speed = -speed * restitution;
                }
            }
        };

        for (size_t atomIndex = 0; atomIndex < atomStorage.mobileCount(); ++atomIndex) {
            confineAxis(atomStorage.posX(atomIndex), atomStorage.velX(atomIndex), max.x);
            confineAxis(atomStorage.posY(atomIndex), atomStorage.velY(atomIndex), max.y);
            confineAxis(atomStorage.posZ(atomIndex), atomStorage.velZ(atomIndex), max.z);
        }
    }

    inline void postProcessVelocities(AtomStorage& atomStorage, float maxSpeed) {
        const float maxSpeedSqr = maxSpeed * maxSpeed;
        float* RESTRICT vx = atomStorage.vxData();
        float* RESTRICT vy = atomStorage.vyData();
        float* RESTRICT vz = atomStorage.vzData();

        const size_t mobileCount = atomStorage.mobileCount();
#pragma GCC ivdep
        for (size_t atomIndex = 0; atomIndex < mobileCount; ++atomIndex) {
            float vxValue = vx[atomIndex];
            float vyValue = vy[atomIndex];
            float vzValue = vz[atomIndex];

            const float speedSqr = vxValue * vxValue + vyValue * vyValue + vzValue * vzValue;
            if (speedSqr > maxSpeedSqr) {
                const float scale = maxSpeed / std::sqrt(speedSqr);
                vxValue *= scale;
                vyValue *= scale;
                vzValue *= scale;
            }

            vx[atomIndex] = vxValue;
            vy[atomIndex] = vyValue;
            vz[atomIndex] = vzValue;
        }
    }

    inline void computeForces(StepData& stepData) {
        PROFILE_SCOPE("StepOps::computeForces");
        stepData.bondsChanged = stepData.forceField.compute(stepData.world, stepData.allowBondFormation, stepData.dt) || stepData.bondsChanged;
    }

    template <typename StepFn>
        requires AtomStepFunc<StepFn>
    inline void predictAndSync(StepData& stepData, StepFn predictFn) {
        AtomStorage& atomStorage = stepData.world.getAtomStorage();

        predictFn(stepData.world.getAtomStorage(), stepData.dt);
        confineToBox(stepData.world);

        atomStorage.swapPrevCurrentForces();
        std::fill_n(atomStorage.fxData(), atomStorage.size(), 0.0f);
        std::fill_n(atomStorage.fyData(), atomStorage.size(), 0.0f);
        std::fill_n(atomStorage.fzData(), atomStorage.size(), 0.0f);
        std::fill_n(atomStorage.energyData(), atomStorage.size(), 0.0f);
    }
}
