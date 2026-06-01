#pragma once

class AtomStorage;
struct StepData;

class KDKScheme {
public:
    void pipeline(StepData& stepData) const;

    static void halfKick(AtomStorage& atomStorage, float accelDamping, float dt);
    static void drift(AtomStorage& atomStorage, float dt);
};
