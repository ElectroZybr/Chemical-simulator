#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>

#include "Lattice/Engine/World.h"
#include "Lattice/Engine/metrics/Profiler.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "Lattice/Engine/physics/IForceField.h"
#include "Lattice/Engine/physics/IIntegrator.h"
#include "Lattice/Engine/physics/IThermostat.h"
#include "Lattice/Engine/restrict.h"

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
            confineAxis(atomStorage.x()[atomIndex], atomStorage.vx()[atomIndex], max.x);
            confineAxis(atomStorage.y()[atomIndex], atomStorage.vy()[atomIndex], max.y);
            confineAxis(atomStorage.z()[atomIndex], atomStorage.vz()[atomIndex], max.z);
        }
    }

    inline void postProcessVelocities(AtomStorage& atomStorage, float maxSpeed) {
        const float maxSpeedSqr = maxSpeed * maxSpeed;
        float* RESTRICT vx = atomStorage.vx().data();
        float* RESTRICT vy = atomStorage.vy().data();
        float* RESTRICT vz = atomStorage.vz().data();

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

    inline void postProcessVelocities(StepContext& stepContext) {
        const float maxSpeed = stepContext.world.getIntegrator().maxParticleSpeed();
        if (maxSpeed > 0.0f) {
            postProcessVelocities(stepContext.world.getAtomStorage(), maxSpeed);
        }
    }

    inline void computeForces(StepContext& stepContext) {
        PROFILE_SCOPE("StepOps::computeForces");
        stepContext.bondsChanged = stepContext.forceField.compute(stepContext.world, stepContext.allowBondFormation, stepContext.dt) || stepContext.bondsChanged;
    }

    inline void applyThermostat(StepContext& stepContext) {
        if (stepContext.thermostat != nullptr) {
            stepContext.thermostat->apply(stepContext);
        }
    }

    template <typename StepFn>
        requires AtomStepFunc<StepFn>
    inline void predictAndSync(StepContext& stepContext, StepFn predictFn) {
        AtomStorage& atomStorage = stepContext.world.getAtomStorage();

        predictFn(stepContext.world.getAtomStorage(), stepContext.dt);
        confineToBox(stepContext.world);

        atomStorage.swapPrevCurrentForces();
        std::fill(atomStorage.fx().begin(), atomStorage.fx().end(), 0.0f);
        std::fill(atomStorage.fy().begin(), atomStorage.fy().end(), 0.0f);
        std::fill(atomStorage.fz().begin(), atomStorage.fz().end(), 0.0f);
        std::fill(atomStorage.energy().begin(), atomStorage.energy().end(), 0.0f);
    }
}
