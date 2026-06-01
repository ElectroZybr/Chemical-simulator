#pragma once

class AtomStorage;
struct StepData;

class VerletScheme {
public:
    void pipeline(StepData& stepData) const;

    static void predict(AtomStorage& atomStorage, float dt);
    static void correct(AtomStorage& atomStorage, float accelDamping, float dt);
};
