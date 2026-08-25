#pragma once

#include <string_view>


class InputAPI {
public:
    virtual bool down(std::string_view trigger) const = 0;
    virtual bool pressed(std::string_view trigger) const = 0;
    virtual bool released(std::string_view trigger) const = 0;
};