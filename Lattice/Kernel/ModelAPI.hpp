#pragma once

#include <string>

struct ModelAPI {
    virtual void update() = 0;
    virtual ~ModelAPI() = default;
};