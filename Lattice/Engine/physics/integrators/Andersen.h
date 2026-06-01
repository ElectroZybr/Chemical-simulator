#pragma once 

#include <random>

class StepData;

class Andersen
{
public:

    Andersen(double temperature, double param, std::mt19937::result_type seed = std::mt19937::default_seed)  :
        t(temperature),
        nu(param),
        randomGenerator(seed)
    {}

    double getTemperature() const { return t; }
    double getParam() const { return nu; }
    void setTemperature(double temperature) { t = temperature; }
    
    void pipeline(StepData& stepData);

    void mkMove(StepData& stepData);

private:

    double t;
    double nu;
    std::mt19937 randomGenerator;
};
