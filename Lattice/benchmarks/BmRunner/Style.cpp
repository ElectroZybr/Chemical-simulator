#include "BmRunner/Style.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <sstream>

#include <unistd.h>
using namespace Lattice;

namespace Benchmarks::BmRunner {
    bool useColor() {
        return ::isatty(STDOUT_FILENO) != 0;
    }

    std::string paint(const std::string& text, const char* color) {
        if (!useColor()) {
            return text;
        }
        return std::string(color) + text + kColorReset;
    }

    std::string paint256(const std::string& text, int colorCode) {
        if (!useColor()) {
            return text;
        }
        return "\033[38;5;" + std::to_string(colorCode) + "m" + text + kColorReset;
    }

    std::string paintRgb(const std::string& text, int r, int g, int b) {
        if (!useColor()) {
            return text;
        }

        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);

        return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m" + text + kColorReset;
    }

    std::string paintNumeric(const std::string& cell, bool cvMode) {
        if (cell.find_first_not_of(" -") == std::string::npos || cell.find('-') != std::string::npos && cell.find_first_not_of(" -") == std::string::npos) {
            return cell;
        }

        if (cvMode) {
            std::smatch match;
            const std::regex pattern(R"([-+]?\d*\.?\d+)");
            if (!std::regex_search(cell, match, pattern)) {
                return paint(cell, kColorIndex);
            }

            const double cv = std::strtod(match[0].str().c_str(), nullptr);
            if (cv < 2.0) {
                return paint(cell, kColorOk);
            }
            if (cv < 5.0) {
                return paint(cell, kColorWarn);
            }
            return paint(cell, kColorBad);
        }

        return paint(cell, kColorIndex);
    }

    std::string paintDelta(const std::string& cell, double neutralThreshold) {
        std::string value = cell;
        value.erase(0, value.find_first_not_of(' '));
        value.erase(value.find_last_not_of(' ') + 1);
        if (value.empty() || value == "-") {
            return cell;
        }

        const double delta = std::strtod(value.c_str(), nullptr);
        if (delta <= -neutralThreshold) {
            return paint(cell, kColorOk);
        }
        if (delta >= neutralThreshold) {
            return paint(cell, kColorBad);
        }
        return paint(cell, kColorHint);
    }

    std::string paintNGradient(const std::string& cell, int minN, int maxN) {
        std::string raw = cell;
        raw.erase(0, raw.find_first_not_of(' '));
        raw.erase(raw.find_last_not_of(' ') + 1);
        if (raw.empty() || !std::all_of(raw.begin(), raw.end(), ::isdigit)) {
            return cell;
        }

        const int n = std::stoi(raw);
        if (maxN <= minN) {
            return paint256(cell, 81);
        }

        const double t = static_cast<double>(n - minN) / static_cast<double>(maxN - minN);
        const int r = static_cast<int>(120 + (170 - 120) * t);
        const int g = static_cast<int>(200 + (165 - 200) * t);
        const int b = static_cast<int>(255 + (245 - 255) * t);
        return paintRgb(cell, r, g, b);
    }

    std::string paintTimeCell(const std::string& cell) {
        if (!useColor()) {
            return cell;
        }

        std::string value = cell;
        while (!value.empty() && value.back() == ' ') {
            value.pop_back();
        }
        const std::string tailSpaces(cell.size() - value.size(), ' ');

        if (value == "-") {
            return cell;
        }
        if (value.size() >= 3 && value.ends_with(" ns")) {
            return value.substr(0, value.size() - 3) + " " + paint("ns", kColorUnitNs) + tailSpaces;
        }
        if (value.size() >= 3 && value.ends_with(" us")) {
            return value.substr(0, value.size() - 3) + " " + paint("us", kColorUnitUs) + tailSpaces;
        }
        if (value.size() >= 3 && value.ends_with(" ms")) {
            return value.substr(0, value.size() - 3) + " " + paint("ms", kColorUnitMs) + tailSpaces;
        }
        return cell;
    }

    std::string paintComplexityLabel(const std::string& cell) {
        std::string raw = cell;
        raw.erase(0, raw.find_first_not_of(' '));
        raw.erase(raw.find_last_not_of(' ') + 1);

        if (raw == "O(1)") {
            return paint(cell, kColorOk);
        }
        if (raw == "O(N)") {
            return paint(cell, "\033[34m");
        }
        if (raw == "O(log N)") {
            return paint(cell, kColorIndexLightBlue);
        }
        if (raw == "O(N log N)") {
            return paint(cell, kColorWarn);
        }
        if (raw.starts_with("O(N^") || raw == "O(N^k)") {
            return paint(cell, kColorBad);
        }
        return cell;
    }

    std::string paintPointsCount(const std::string& cell) {
        std::string raw = cell;
        raw.erase(0, raw.find_first_not_of(' '));
        raw.erase(raw.find_last_not_of(' ') + 1);
        if (raw.empty() || !std::all_of(raw.begin(), raw.end(), ::isdigit)) {
            return cell;
        }

        const int count = std::stoi(raw);
        if (count <= 3) {
            return paint(cell.substr(0, cell.find_last_not_of(' ') + 1) + " (!)", kColorBad);
        }
        if (count <= 5) {
            return paint(cell, kColorWarn);
        }
        return paint(cell, kColorOk);
    }

    std::string paintR2Cell(const std::string& cell) {
        std::string raw = cell;
        raw.erase(0, raw.find_first_not_of(' '));
        raw.erase(raw.find_last_not_of(' ') + 1);
        if (raw.empty() || raw == "-") {
            return cell;
        }

        const double r2 = std::strtod(raw.c_str(), nullptr);
        if (r2 >= 0.95) {
            return paint(cell, kColorOk);
        }
        if (r2 >= 0.85) {
            return paint(cell, kColorWarn);
        }
        return paint(cell, kColorBad);
    }
}
