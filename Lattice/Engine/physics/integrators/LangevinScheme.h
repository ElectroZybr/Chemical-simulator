#pragma once

class AtomStorage;
struct StepData;

class LangevinScheme {
public:
    void pipeline(StepData& stepData) const;
};
