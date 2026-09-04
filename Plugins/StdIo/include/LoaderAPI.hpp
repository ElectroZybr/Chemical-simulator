#pragma once

// #include "Lattice/Kernel/Node.hpp"

#include <string_view>

class Value;

class LoaderAPI {
public:
    virtual std::string_view section() const = 0;
    virtual void load(const Value* data) = 0;//, Lattice::Node ctx
};