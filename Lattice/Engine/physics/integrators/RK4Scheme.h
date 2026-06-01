#pragma once

class AtomStorage;
struct StepData;

class RK4Scheme {
public:
    void pipeline(StepData& stepData) const;
};
