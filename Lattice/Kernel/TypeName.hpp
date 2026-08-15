#pragma once
#include <string_view>

namespace Lattice {

template<typename T>
constexpr std::string_view typeName() {
    constexpr std::string_view p = __PRETTY_FUNCTION__;

#if defined(__clang__)
    constexpr std::string_view prefix = "std::string_view Lattice::typeName() [T = ";
    constexpr auto start = prefix.size();
    constexpr auto end = p.find(']', start);
#elif defined(__GNUC__)
    constexpr std::string_view prefix = "constexpr std::string_view Lattice::typeName() [with T = ";
    constexpr auto start = prefix.size();
    constexpr auto end = p.find(';', start);
#else
#error "typeName: unsupported compiler"
#endif

    constexpr auto full = p.substr(start, end - start);

    constexpr auto pos = full.rfind("::");
    if (pos == std::string_view::npos)
        return full;

    return full.substr(pos + 2);
}

} // namespace Lattice