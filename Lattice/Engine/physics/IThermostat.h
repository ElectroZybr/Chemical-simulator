#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

#include "Lattice/Engine/StepContext.h"
#include "Lattice/Engine/ModuleRegistry.hpp"

class IThermostat : public IModule {
public:
    virtual ~IThermostat() = default;
    virtual void setTemperature(float temperature) { (void)temperature; }
    virtual float temperature() const { return 0.0f; }
    virtual void setParam(float param) { (void)param; }
    virtual float param() const { return 0.0f; }
    virtual void apply(StepContext& stepContext) = 0;
};

class Thermostat : public RegisteredModuleOwner<Thermostat, IThermostat> {
public:
    static constexpr bool allowEmpty = true;

    Thermostat() = default;
    Thermostat(const Thermostat&) = delete;
    Thermostat& operator=(const Thermostat&) = delete;
    Thermostat(Thermostat&&) noexcept = default;
    Thermostat& operator=(Thermostat&&) noexcept = default;

    static ModuleRegistry2<IThermostat>& registry();

    bool setThermostat(std::string_view id) { return set(id); }
    std::string_view getThermostat() const { return get(); }
    IThermostat* activeThermostat() const { return active(); }

    void setTemperature(float temperature) {
        temperature_ = std::max(0.0f, temperature);
        if (impl_) {
            impl_->setTemperature(temperature_);
        }
    }
    float temperature() const { return temperature_; }

    void setParam(float param) {
        param_ = std::max(0.0f, param);
        if (impl_) {
            impl_->setParam(param_);
        }
    }
    float param() const { return param_; }

    void onModuleSet(IThermostat& thermostat) {
        thermostat.setTemperature(temperature_);
        thermostat.setParam(param_);
    }

private:
    float temperature_ = 300.0f;
    float param_ = 100.0f;
};
