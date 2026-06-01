#include "Andersen.h"

#include "Engine/metrics/Profiler.h"
#include "Engine/physics/integrators/StepOps.h"
#include "Engine/physics/integrators/VerletScheme.h"

void Andersen::pipeline(StepData& stepData)
{
    PROFILE_SCOPE("Andersen::pipeline");
    
    StepOps::predictAndSync(stepData, &VerletScheme::predict);
    StepOps::computeForces(stepData);
    VerletScheme::correct(stepData.world.getAtomStorage(), 1.0f, stepData.dt);
    mkMove(stepData);
}

void Andersen::mkMove(StepData& stepData)
{
    AtomStorage& atomStorage = stepData.world.getAtomStorage();
    const double probability = std::clamp(stepData.dt * nu, 0.0, 1.0);
    std::uniform_real_distribution<double> uniDist(0.0, 1.0);
    const size_t mobileCount = atomStorage.mobileCount();

    for (size_t i = 0; i < mobileCount; ++i)
    {
        const double randomVal = uniDist(randomGenerator);

        if(randomVal < probability)
        {
            const float m = AtomData::getProps(atomStorage.type(i)).mass;
            const float sigma = std::sqrt(Units::kboltzmann * t / m);
            std::normal_distribution<float> Maksvell(0.f, sigma);

            atomStorage.velX(i) = Maksvell(randomGenerator);
            atomStorage.velY(i) = Maksvell(randomGenerator);
            atomStorage.velZ(i) = Maksvell(randomGenerator);
        }
    }
}
