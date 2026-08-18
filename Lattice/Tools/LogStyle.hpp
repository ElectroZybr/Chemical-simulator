#pragma once

#include <string>
#include <string_view>

namespace LogStyle {
namespace Color {
inline constexpr std::string_view reset = "\033[0m";
inline constexpr std::string_view bold = "\033[1m";
inline constexpr std::string_view tree = "\033[90m";
inline constexpr std::string_view logo = "\033[96m";
inline constexpr std::string_view header = "\033[96m";
inline constexpr std::string_view key = "\033[94m";
inline constexpr std::string_view value = "\033[97m";
inline constexpr std::string_view device = "\033[93m";
inline constexpr std::string_view error = "\033[91m";
inline constexpr std::string_view ok = "\033[92m";
inline constexpr std::string_view warning = "\033[93m";
inline constexpr std::string_view prompt = "\033[95m";
} // namespace Color

inline std::string paint(std::string_view text, std::string_view color) {
    std::string out;
    out.reserve(color.size() + text.size() + Color::reset.size());
    out += color;
    out += text;
    out += Color::reset;
    return out;
}
}
