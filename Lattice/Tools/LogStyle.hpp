#pragma once

#include <string>
#include <string_view>


namespace Color {

inline constexpr std::string_view reset = "\033[0m";
inline constexpr std::string_view bold  = "\033[1m";

// Base colors
inline constexpr std::string_view black   = "\033[30m";
inline constexpr std::string_view red     = "\033[31m";
inline constexpr std::string_view green   = "\033[32m";
inline constexpr std::string_view yellow  = "\033[33m";
inline constexpr std::string_view blue    = "\033[34m";
inline constexpr std::string_view magenta = "\033[35m";
inline constexpr std::string_view cyan    = "\033[36m";
inline constexpr std::string_view white   = "\033[37m";

// Bright colors
inline constexpr std::string_view gray          = "\033[90m";
inline constexpr std::string_view brightRed     = "\033[91m";
inline constexpr std::string_view brightGreen   = "\033[92m";
inline constexpr std::string_view brightYellow  = "\033[93m";
inline constexpr std::string_view brightBlue    = "\033[94m";
inline constexpr std::string_view brightMagenta = "\033[95m";
inline constexpr std::string_view brightCyan    = "\033[96m";
inline constexpr std::string_view brightWhite   = "\033[97m";

// Semantic styles
inline constexpr std::string_view error   = brightRed;
inline constexpr std::string_view ok      = brightGreen;
inline constexpr std::string_view warning = brightYellow;
inline constexpr std::string_view prompt  = brightMagenta;


inline std::string paint(std::string_view text, std::string_view color) {
    std::string out;
    out.reserve(color.size() + text.size() + Color::reset.size());
    out += color;
    out += text;
    out += Color::reset;
    return out;
}
}

