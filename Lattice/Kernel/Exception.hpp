#pragma once

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace Lattice {

class Exception : public std::runtime_error {
public:
    template<typename... TArgs>
    Exception(std::string_view tag, std::format_string<TArgs...> format, TArgs&&... args)
        : std::runtime_error(std::format(format, std::forward<TArgs>(args)...))
        , tag_(tag) {}

    std::string_view tag() const noexcept {
        return tag_;
    }

private:
    std::string tag_;
};
}