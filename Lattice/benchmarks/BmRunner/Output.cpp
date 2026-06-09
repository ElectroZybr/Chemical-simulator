#include "BmRunner/Output.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

#include "BmRunner/Data.h"
#include "BmRunner/Style.h"
#include "BmRunner/Support.h"
using namespace Lattice;

namespace fs = std::filesystem;

namespace Benchmarks::BmRunner {
    namespace {
        std::size_t visibleWidth(std::string_view text) {
            std::size_t width = 0;
            for (std::size_t i = 0; i < text.size(); ++i) {
                const unsigned char ch = static_cast<unsigned char>(text[i]);
                if (ch == '\033' && i + 1 < text.size() && text[i + 1] == '[') {
                    i += 2;
                    while (i < text.size() && text[i] != 'm') {
                        ++i;
                    }
                    continue;
                }
                if ((ch & 0xC0) != 0x80) {
                    ++width;
                }
            }
            return width;
        }

        std::string padRight(std::string_view text, std::size_t width) {
            std::string out(text);
            const std::size_t current = visibleWidth(out);
            if (current < width) {
                out.append(width - current, ' ');
            }
            return out;
        }

        struct TableRow {
            std::string index;
            std::string test;
            std::string n;
            std::string cv;
            std::string ips;
            std::string time;
            std::string baseline;
            std::string delta;
        };

        struct FitResult {
            double slope = 0.0;
            double intercept = 0.0;
            double r2 = 0.0;
        };

        std::string formatDouble(double value, int digits) {
            std::ostringstream out;
            out << std::fixed << std::setprecision(digits) << value;
            return out.str();
        }

        std::string fmtCv(const std::optional<double>& value) {
            if (!value) {
                return "-";
            }
            std::string text = formatDouble(*value, 2);
            if (*value >= 5.0) {
                text += " !";
            }
            return text;
        }

        std::string fmtTimeNs(const std::optional<double>& value) {
            if (!value) {
                return "-";
            }
            const double absValue = std::abs(*value);
            if (absValue >= 1'000'000.0) {
                return formatDouble(*value / 1'000'000.0, 2) + " ms";
            }
            if (absValue >= 1'000.0) {
                return formatDouble(*value / 1'000.0, 2) + " us";
            }
            return formatDouble(*value, 2) + " ns";
        }

        std::string fmtIps(const std::optional<double>& value) {
            if (!value) {
                return "-";
            }
            const double absValue = std::abs(*value);
            if (absValue >= 1'000'000'000.0) {
                return formatDouble(*value / 1'000'000'000.0, 2) + "G";
            }
            if (absValue >= 1'000'000.0) {
                return formatDouble(*value / 1'000'000.0, 2) + "M";
            }
            if (absValue >= 1'000.0) {
                return formatDouble(*value / 1'000.0, 2) + "k";
            }
            return formatDouble(*value, 2);
        }

        std::pair<std::string, std::string> compareStatus(const std::optional<double>& currentMedian,
                                                          const std::optional<double>& baselineMedian) {
            if (!currentMedian || !baselineMedian || *baselineMedian <= 0.0) {
                return {"-", ""};
            }

            const double deltaPct = (*currentMedian - *baselineMedian) / *baselineMedian * 100.0;
            std::ostringstream out;
            out << std::showpos << std::fixed << std::setprecision(1) << deltaPct << "%";
            return {out.str(), ""};
        }

        std::string estimateComplexityLabel(double alpha) {
            if (alpha < 0.25) {
                return "O(1)";
            }
            if (alpha < 0.75) {
                return "O(log N)";
            }
            if (alpha < 1.35) {
                return "O(N)";
            }
            if (alpha < 1.75) {
                return "O(N log N)";
            }
            if (alpha < 2.35) {
                return "O(N^2)";
            }
            if (alpha < 2.85) {
                return "O(N^3)";
            }
            return "O(N^k)";
        }

        FitResult linearFit(const std::vector<double>& xs, const std::vector<double>& ys) {
            const std::size_t n = xs.size();
            if (n < 2) {
                return {};
            }

            const double meanX = std::accumulate(xs.begin(), xs.end(), 0.0) / static_cast<double>(n);
            const double meanY = std::accumulate(ys.begin(), ys.end(), 0.0) / static_cast<double>(n);

            double ssXx = 0.0;
            double ssXy = 0.0;
            double ssTot = 0.0;
            double ssRes = 0.0;

            for (std::size_t i = 0; i < n; ++i) {
                ssXx += (xs[i] - meanX) * (xs[i] - meanX);
                ssXy += (xs[i] - meanX) * (ys[i] - meanY);
            }

            if (ssXx <= 0.0) {
                return {};
            }

            const double slope = ssXy / ssXx;
            const double intercept = meanY - slope * meanX;

            for (std::size_t i = 0; i < n; ++i) {
                const double predicted = slope * xs[i] + intercept;
                ssTot += (ys[i] - meanY) * (ys[i] - meanY);
                ssRes += (ys[i] - predicted) * (ys[i] - predicted);
            }

            const double r2 = ssTot > 0.0 ? 1.0 - (ssRes / ssTot) : 1.0;
            return {.slope = slope, .intercept = intercept, .r2 = r2};
        }

        void printComplexityEstimate(const std::unordered_map<std::string, RowMetrics>& rows,
                                     const std::unordered_map<std::string, BenchmarkMeta>& metadata,
                                     std::string_view sceneKey,
                                     std::string_view degradationKey) {
            if (degradationKey != "size") {
                return;
            }

            std::unordered_map<std::string, std::vector<std::pair<int, double>>> grouped;

            for (const auto& [runName, metrics] : rows) {
                const std::string arg = benchArg(runName);
                if (!std::all_of(arg.begin(), arg.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
                    continue;
                }

                std::optional<double> timeValue = metrics.realMedian ? metrics.realMedian : metrics.realMean;
                if (!timeValue || *timeValue <= 0.0) {
                    continue;
                }

                grouped[shortBenchId(runName)].push_back({atomCountForSceneKey(sceneKey, std::stoi(arg)), *timeValue});
            }

            struct ComplexityRow {
                std::string name;
                double alpha = 0.0;
                double r2 = 0.0;
                int count = 0;
            };

            std::vector<ComplexityRow> tableRows;
            for (auto& [benchId, points] : grouped) {
                std::sort(points.begin(), points.end());
                points.erase(std::unique(points.begin(), points.end()), points.end());
                if (points.size() < 3) {
                    continue;
                }

                std::vector<double> xs;
                std::vector<double> ys;
                for (const auto& [n, time] : points) {
                    xs.push_back(std::log(static_cast<double>(n)));
                    ys.push_back(std::log(time));
                }

                const FitResult fit = linearFit(xs, ys);
                auto it = metadata.find(benchId);
                if (it == metadata.end()) {
                    it = metadata.find(metaLookupId(benchId));
                }
                tableRows.push_back({
                    .name = it != metadata.end() ? it->second.label : benchId,
                    .alpha = fit.slope,
                    .r2 = fit.r2,
                    .count = static_cast<int>(points.size()),
                });
            }

            if (tableRows.empty()) {
                return;
            }

            std::sort(tableRows.begin(), tableRows.end(), [](const ComplexityRow& lhs, const ComplexityRow& rhs) {
                return lhs.name < rhs.name;
            });

            std::vector<std::array<std::string, 5>> cells;
            std::array<std::size_t, 5> widths{
                visibleWidth("Test"),
                visibleWidth("alpha"),
                visibleWidth("Complexity"),
                visibleWidth("R2"),
                visibleWidth("Points"),
            };

            for (const auto& row : tableRows) {
                cells.push_back({
                    row.name,
                    formatDouble(row.alpha, 2),
                    estimateComplexityLabel(row.alpha),
                    formatDouble(row.r2, 3),
                    std::to_string(row.count),
                });

                for (std::size_t i = 0; i < widths.size(); ++i) {
                    widths[i] = std::max(widths[i], visibleWidth(cells.back()[i]));
                }
            }

            std::cout << '\n' << paint("Complexity Estimate:", kColorTitle) << '\n';
            std::cout << "  "
                      << padRight("Test", widths[0]) << " | "
                      << padRight("alpha", widths[1]) << " | "
                      << padRight("Complexity", widths[2]) << " | "
                      << padRight("R2", widths[3]) << " | "
                      << padRight("Points", widths[4]) << '\n';
            std::cout << "  "
                      << std::string(widths[0], '-') << "-+-"
                      << std::string(widths[1], '-') << "-+-"
                      << std::string(widths[2], '-') << "-+-"
                      << std::string(widths[3], '-') << "-+-"
                      << std::string(widths[4], '-') << '\n';

            for (const auto& row : cells) {
                std::string name = row[0];
                std::string alpha = row[1];
                std::string complexity = row[2];
                std::string r2 = row[3];
                std::string points = row[4];

                name = padRight(name, widths[0]);
                alpha = padRight(alpha, widths[1]);
                complexity = padRight(complexity, widths[2]);
                r2 = padRight(r2, widths[3]);
                points = padRight(points, widths[4]);

                complexity = paintComplexityLabel(complexity);
                r2 = paintR2Cell(r2);
                points = paintPointsCount(points);

                std::cout << "  " << name << " | " << alpha << " | " << complexity << " | " << r2 << " | " << points << '\n';
            }
        }
    }

    void printResultsTable(const BenchmarkData& data,
                           const std::unordered_map<std::string, BenchmarkMeta>& metadata,
                           const fs::path& resultsPath,
                           std::string_view sceneKey,
                           std::string_view degradationKey) {
        auto rows = buildRows(data);
        if (rows.empty()) {
            std::cout << "No data to display.\n";
            return;
        }

        const fs::path baselinePath = resultsPath / "baseline.json";
        const bool allowBaseline = degradationKey == "size";
        const auto baselineData = allowBaseline ? loadBenchmarkDataIfExists(baselinePath) : std::optional<BenchmarkData>{};
        const auto baselineRows = baselineData ? buildRows(*baselineData) : std::unordered_map<std::string, RowMetrics>{};
        const bool hasBaseline = baselineData.has_value();

        std::vector<std::pair<std::string, RowMetrics>> ordered(rows.begin(), rows.end());
        std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
            const std::string lhsId = shortBenchId(lhs.first);
            const std::string rhsId = shortBenchId(rhs.first);
            if (lhsId != rhsId) {
                return lhsId < rhsId;
            }

            const std::string lhsArg = benchArg(lhs.first);
            const std::string rhsArg = benchArg(rhs.first);
            const bool lhsNum = !lhsArg.empty() && std::all_of(lhsArg.begin(), lhsArg.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
            const bool rhsNum = !rhsArg.empty() && std::all_of(rhsArg.begin(), rhsArg.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
            if (lhsNum && rhsNum) {
                return std::stoi(lhsArg) < std::stoi(rhsArg);
            }
            return lhsArg < rhsArg;
        });

        std::vector<TableRow> table;
        const std::string nHeader = degradationKey == "time" ? "Age" : "N";
        std::array<std::size_t, 8> widths{
            visibleWidth("#"),
            visibleWidth("Test"),
            visibleWidth(nHeader),
            visibleWidth("cv(%)"),
            visibleWidth("items/s"),
            visibleWidth("time"),
            visibleWidth("baseline"),
            visibleWidth("d (%)"),
        };

        int index = 1;
        for (const auto& [runName, metrics] : ordered) {
            const std::string benchId = shortBenchId(runName);
            auto metaIt = metadata.find(benchId);
            if (metaIt == metadata.end()) {
                metaIt = metadata.find(metaLookupId(benchId));
            }
            const auto baseIt = baselineRows.find(runName);
            const std::string extentArg = benchArg(runName);
            std::string nDisplay = extentArg;
            if (!extentArg.empty() && std::all_of(extentArg.begin(), extentArg.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
                const int numericArg = std::stoi(extentArg);
                if (degradationKey == "time") {
                    nDisplay = std::to_string(temporalAgeStepsFromArg(numericArg));
                } else {
                    nDisplay = std::to_string(atomCountForSceneKey(sceneKey, numericArg));
                }
            }
            const std::optional<double> currentTime = metrics.realMedian ? metrics.realMedian : metrics.realMean;
            const std::optional<double> baseTime = baseIt != baselineRows.end()
                ? (baseIt->second.realMedian ? baseIt->second.realMedian : baseIt->second.realMean)
                : std::optional<double>{};
            const auto [deltaText, _] = compareStatus(currentTime, baseTime);

            TableRow row{
                .index = std::to_string(index++),
                .test = metaIt != metadata.end() ? metaIt->second.label : benchId,
                .n = nDisplay,
                .cv = fmtCv(metrics.realCv),
                .ips = fmtIps(metrics.ips),
                .time = fmtTimeNs(currentTime),
                .baseline = fmtTimeNs(baseTime),
                .delta = deltaText,
            };

            widths[0] = std::max(widths[0], visibleWidth(row.index));
            widths[1] = std::max(widths[1], visibleWidth(row.test));
            widths[2] = std::max(widths[2], visibleWidth(row.n));
            widths[3] = std::max(widths[3], visibleWidth(row.cv));
            widths[4] = std::max(widths[4], visibleWidth(row.ips));
            widths[5] = std::max(widths[5], visibleWidth(row.time));
            widths[6] = std::max(widths[6], visibleWidth(row.baseline));
            widths[7] = std::max(widths[7], visibleWidth(row.delta));
            table.push_back(std::move(row));
        }

        std::vector<int> nValues;
        for (const auto& row : table) {
            if (!row.n.empty() && std::all_of(row.n.begin(), row.n.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
                nValues.push_back(std::stoi(row.n));
            }
        }
        const int minN = nValues.empty() ? 0 : *std::min_element(nValues.begin(), nValues.end());
        const int maxN = nValues.empty() ? 0 : *std::max_element(nValues.begin(), nValues.end());

        if (hasBaseline) {
            std::cout << '\n' << paint("baseline found", kColorOk) << '\n';
        } else {
            std::cout << '\n' << paint("baseline not found", kColorError) << '\n';
        }
        std::cout << paint("Results Table:", kColorTitle) << '\n';
        std::cout << "  "
                  << padRight("#", widths[0]) << " | "
                  << padRight("Test", widths[1]) << " | "
                  << padRight(nHeader, widths[2]) << " | "
                  << padRight("cv(%)", widths[3]) << " | "
                  << padRight("items/s", widths[4]) << " | "
                  << padRight("time", widths[5]);
        if (hasBaseline) {
            std::cout << " | "
                      << padRight("baseline", widths[6]) << " | "
                      << padRight("d (%)", widths[7]);
        }
        std::cout << '\n';
        std::cout << "  "
                  << std::string(widths[0], '-') << "-+-"
                  << std::string(widths[1], '-') << "-+-"
                  << std::string(widths[2], '-') << "-+-"
                  << std::string(widths[3], '-') << "-+-"
                  << std::string(widths[4], '-') << "-+-"
                  << std::string(widths[5], '-');
        if (hasBaseline) {
            std::cout << "-+-"
                      << std::string(widths[6], '-') << "-+-"
                      << std::string(widths[7], '-');
        }
        std::cout << '\n';

        for (const auto& row : table) {
            std::string index = row.index;
            std::string test = row.test;
            std::string n = row.n;
            std::string cv = row.cv;
            std::string ips = row.ips;
            std::string time = row.time;
            std::string baseline = row.baseline;
            std::string delta = row.delta;

            index = padRight(index, widths[0]);
            test = padRight(test, widths[1]);
            n = padRight(n, widths[2]);
            cv = padRight(cv, widths[3]);
            ips = padRight(ips, widths[4]);
            time = padRight(time, widths[5]);
            baseline = padRight(baseline, widths[6]);
            delta = padRight(delta, widths[7]);

            std::cout << "  "
                      << paint(index, kColorIndexLightBlue) << " | "
                      << test << " | "
                      << paintNGradient(n, minN, maxN) << " | "
                      << paintNumeric(cv, true) << " | "
                      << ips << " | "
                      << paintTimeCell(time);
            if (hasBaseline) {
                std::cout << " | "
                          << paintTimeCell(baseline) << " | "
                          << paintDelta(delta);
            }
            std::cout << '\n';
        }

        printComplexityEstimate(rows, metadata, sceneKey, degradationKey);
    }
}
