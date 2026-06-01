#pragma once

#include <string>

namespace Benchmarks::BmRunner {
    bool useColor();

    std::string paint(const std::string& text, const char* color);
    std::string paint256(const std::string& text, int colorCode);
    std::string paintRgb(const std::string& text, int r, int g, int b);

    std::string paintNumeric(const std::string& cell, bool cvMode = false);
    std::string paintDelta(const std::string& cell, double neutralThreshold = 2.5);
    std::string paintNGradient(const std::string& cell, int minN, int maxN);
    std::string paintTimeCell(const std::string& cell);
    std::string paintComplexityLabel(const std::string& cell);
    std::string paintPointsCount(const std::string& cell);
    std::string paintR2Cell(const std::string& cell);

    inline constexpr const char* kColorReset = "\033[0m";
    inline constexpr const char* kColorTitle = "\033[1;97m";
    inline constexpr const char* kColorLabelWhite = "\033[37m";
    inline constexpr const char* kColorIndex = "\033[33m";
    inline constexpr const char* kColorIndexLightBlue = "\033[96m";
    inline constexpr const char* kColorHint = "\033[90m";
    inline constexpr const char* kColorError = "\033[31m";
    inline constexpr const char* kColorOk = "\033[32m";
    inline constexpr const char* kColorWarn = "\033[33m";
    inline constexpr const char* kColorBad = "\033[31m";
    inline constexpr const char* kColorUnitNs = "\033[90m";
    inline constexpr const char* kColorUnitUs = "\033[36m";
    inline constexpr const char* kColorUnitMs = "\033[35m";
    inline constexpr const char* kColorProgressSpinner = "\033[96m";
    inline constexpr const char* kColorProgressBar = "\033[36m";
    inline constexpr const char* kColorProgressCount = "\033[33m";
    inline constexpr const char* kColorProgressCase = "\033[95m";
    inline constexpr const char* kColorProgressTime = "\033[92m";
    inline constexpr const char* kColorMenuGrid = "\033[38;5;240m";
    inline constexpr const char* kColorMenuCategory = "\033[1;96m";
    inline constexpr const char* kColorMenuCategoryLabel = "\033[1;33m";
}
