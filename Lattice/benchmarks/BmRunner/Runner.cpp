#include "BmRunner/Runner.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <unistd.h>

#include "BmRunner/Data.h"
#include "BmRunner/Output.h"
#include "BmRunner/Style.h"
#include "BmRunner/Support.h"
using namespace Lattice;

namespace fs = std::filesystem;

namespace Benchmarks::BmRunner {
    namespace {
        constexpr std::string_view kMenuIndent = "  ";
        constexpr std::string_view kBannerIndent = "   ";
        constexpr std::size_t kBannerWidth = 68;

        static void printBanner() {
            static constexpr std::array<std::string_view, 5> kBannerLines{
                "    __                    __                         __       ",
                "   / /_  ___  ____  _____/ /_  ____ ___  ____ ______/ /_______",
                "  / __ \\/ _ \\/ __ \\/ ___/ __ \\/ __ `__ \\/ __ `/ ___/ //_/ ___/",
                " / /_/ /  __/ / / / /__/ / / / / / / / / /_/ / /  / ,< (__  ) ",
                "/_.___/\\___/_/ /_/\\___/_/ /_/_/ /_/ /_/\\__,_/_/  /_/|_/____/  ",
            };

            for (const auto& line : kBannerLines) {
                std::cout << kBannerIndent << paint(std::string(line), "\033[36m") << '\n';
            }
            std::cout << '\n';
        }

        struct RunProfile {
            std::string_view label;
            int repetitions;
            std::string_view minTime;
        };

        constexpr std::array kRunProfiles{
            RunProfile{"Quick", 1, "0.1s"},
            RunProfile{"Medium", 3, "0.5s"},
            RunProfile{"Accurate", 7, "1s"},
        };

        struct CommandOutput {
            int exitCode = 0;
            std::string stdoutText;
            std::string stderrText;
        };

        std::string safeFilterSuffix(std::string text) {
            for (char& ch : text) {
                if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) {
                    ch = '_';
                }
            }

            while (text.find("__") != std::string::npos) {
                text.replace(text.find("__"), 2, "_");
            }

            while (!text.empty() && (text.front() == '_' || text.front() == '.')) {
                text.erase(text.begin());
            }
            while (!text.empty() && (text.back() == '_' || text.back() == '.')) {
                text.pop_back();
            }

            return text.empty() ? "filtered" : text;
        }

        fs::path saveResult(const fs::path& results, const fs::path& sourceJson, const std::string& filter) {
            const auto now = std::chrono::system_clock::now();
            const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
            std::tm localTime{};
            localtime_r(&nowTime, &localTime);

            std::ostringstream name;
            name << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S");
            name << "_" << (filter.empty() ? "all" : safeFilterSuffix(filter)) << ".json";

            const fs::path target = results / name.str();
            std::error_code ec;
            fs::copy_file(sourceJson, target, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                die("Failed to save result: " + ec.message());
            }

            std::cout << "Saved: " << target.string() << '\n';
            return target;
        }

        std::string normalizeBenchmarkCase(std::string name) {
            for (const std::string_view suffix : {"_mean", "_median", "_stddev", "_cv"}) {
                if (name.size() >= suffix.size() && name.ends_with(suffix)) {
                    name.erase(name.size() - suffix.size());
                    break;
                }
            }
            return name;
        }

        std::string formatEta(std::optional<double> seconds) {
            if (!seconds || *seconds < 0.0 || !std::isfinite(*seconds)) {
                return "--:--";
            }

            int total = static_cast<int>(std::lround(*seconds));
            const int s = total % 60;
            total /= 60;
            const int m = total % 60;
            const int h = total / 60;

            std::ostringstream out;
            out << std::setfill('0');
            if (h > 0) {
                out << std::setw(2) << h << ":" << std::setw(2) << m << ":" << std::setw(2) << s;
            } else {
                out << std::setw(2) << m << ":" << std::setw(2) << s;
            }
            return out.str();
        }

        std::string progressBar(int fill, int width) {
            std::string bar;
            for (int i = 0; i < fill; ++i) {
                bar += "█";
            }
            for (int i = fill; i < width; ++i) {
                bar += "░";
            }
            return bar;
        }

        void redrawProgress(int done,
                            int total,
                            const std::string& currentCase,
                            std::optional<std::chrono::steady_clock::time_point> startTs,
                            int tick) {
            if (!useColor() || total <= 0) {
                return;
            }

            static const std::vector<std::string> spinnerFrames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
            const std::string spin = spinnerFrames[static_cast<std::size_t>(tick) % spinnerFrames.size()];
            constexpr int width = 24;
            const double ratio = std::clamp(total > 0 ? static_cast<double>(done) / static_cast<double>(total) : 0.0, 0.0, 1.0);
            const int fill = static_cast<int>(width * ratio);

            std::optional<double> etaSeconds;
            std::optional<double> elapsedSeconds;
            if (startTs) {
                const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - *startTs).count();
                elapsedSeconds = elapsed;
                if (done > 0) {
                    const double perTest = elapsed / static_cast<double>(done);
                    etaSeconds = perTest * static_cast<double>(std::max(0, total - done));
                }
            }

            std::string caseText = currentCase.empty() ? "-" : currentCase;
            if (caseText.size() > 64) {
                caseText = caseText.substr(0, 61) + "...";
            }

            const std::string spinCol = paint(spin, kColorProgressSpinner);
            const std::string barCol = paint(progressBar(fill, width), kColorProgressBar);
            const std::string countCol = paint("[" + std::to_string(done) + "/" + std::to_string(total) + "]", kColorProgressCount);
            const std::string caseCol = paint(caseText, kColorProgressCase);
            const std::string etaCol = paint(formatEta(etaSeconds), kColorProgressTime);
            const std::string elapsedCol = paint(formatEta(elapsedSeconds), kColorProgressTime);
            const std::string text = spinCol + " [" + barCol + "] " + countCol + " | " + caseCol + " | Elapsed " + elapsedCol + " | ETA " + etaCol;

            std::cout << "\r\033[2K" << paint(text, kColorHint);
            std::cout.flush();
        }

        CommandOutput runCommandCapture(const std::string& command, const fs::path& workDir, const fs::path& resultsPath, const std::string& stem) {
            const fs::path stdoutPath = resultsPath / ("_" + stem + ".stdout.txt");
            const fs::path stderrPath = resultsPath / ("_" + stem + ".stderr.txt");
            const std::string shellCommand =
                "cd " + quoteShell(workDir.string()) + " && " + command + " > " + quoteShell(stdoutPath.string()) + " 2> " + quoteShell(stderrPath.string());

            CommandOutput output;
            output.exitCode = std::system(shellCommand.c_str());
            if (fs::exists(stdoutPath)) {
                output.stdoutText = readTextFile(stdoutPath);
                std::error_code ec;
                fs::remove(stdoutPath, ec);
            }
            if (fs::exists(stderrPath)) {
                output.stderrText = readTextFile(stderrPath);
                std::error_code ec;
                fs::remove(stderrPath, ec);
            }
            return output;
        }

        std::vector<std::string> listBenchmarkCases(const fs::path& binary,
                                                    const std::string& filterRegex,
                                                    const fs::path& benchmarksRoot) {
            const fs::path resultsPath = resultsDir(benchmarksRoot);
            ensureResultsDir(resultsPath);
            const CommandOutput output = runCommandCapture(
                quoteShell(binary.string()) + " --benchmark_list_tests=true",
                benchmarksRoot,
                resultsPath,
                "list_tests");

            if (output.exitCode != 0) {
                return {};
            }

            std::vector<std::string> names;
            std::regex rx;
            bool useRegex = false;
            if (!filterRegex.empty()) {
                try {
                    rx = std::regex(filterRegex);
                    useRegex = true;
                } catch (const std::regex_error&) {
                    useRegex = false;
                }
            }

            std::istringstream lines(output.stdoutText);
            std::string line;
            while (std::getline(lines, line)) {
                if (line.empty()) {
                    continue;
                }
                if (useRegex && !std::regex_search(line, rx)) {
                    continue;
                }
                names.push_back(line);
            }
            return names;
        }

        std::string prettyFilterName(const std::string& filterName,
                                     const std::unordered_map<std::string, BenchmarkMeta>& metadata) {
            auto it = metadata.find(filterName);
            if (it == metadata.end()) {
                it = metadata.find(metaLookupId(filterName));
            }
            if (it != metadata.end()) {
                return it->second.label;
            }

            const auto slash = filterName.find('/');
            return slash == std::string::npos ? filterName : filterName.substr(slash + 1);
        }

        std::pair<std::string, std::string> classifyByMetadata(
            const std::string& filterName,
            const std::unordered_map<std::string, BenchmarkMeta>& metadata) {
            auto it = metadata.find(filterName);
            if (it == metadata.end()) {
                it = metadata.find(metaLookupId(filterName));
            }
            if (it == metadata.end()) {
                return {"Other", ""};
            }

            const std::string& group = it->second.group;
            const auto slash = group.find('/');
            if (slash == std::string::npos) {
                return {group.empty() ? "Other" : group, ""};
            }
            return {group.substr(0, slash), group.substr(slash + 1)};
        }

        std::size_t visibleWidth(std::string_view text) {
            std::size_t width = 0;
            for (std::size_t i = 0; i < text.size(); ++i) {
                const unsigned char ch = static_cast<unsigned char>(text[i]);
                if ((ch & 0xC0u) != 0x80u) {
                    ++width;
                }
            }
            return width;
        }

        int terminalWidth() {
            if (const char* columns = std::getenv("COLUMNS")) {
                try {
                    const int parsed = std::stoi(columns);
                    if (parsed >= 60) {
                        return parsed;
                    }
                } catch (const std::exception&) {
                }
            }
            return 120;
        }

        void printMenuEntriesColumns(const std::vector<std::pair<std::string, std::string>>& entries,
                                     int& menuIndex,
                                     std::vector<std::string>& menuFilters) {
            if (entries.empty()) {
                return;
            }

            std::vector<std::string> plainCells;
            plainCells.reserve(entries.size());

            std::size_t maxCellWidth = 0;
            int previewIndex = menuIndex;
            for (const auto& [_, pretty] : entries) {
                std::string cell = std::to_string(previewIndex) + ") " + pretty;
                maxCellWidth = std::max(maxCellWidth, visibleWidth(cell));
                plainCells.push_back(std::move(cell));
                ++previewIndex;
            }

            const std::size_t gutter = 4;
            const std::size_t columnWidth = maxCellWidth + gutter;
            const int width = terminalWidth();
            int columns = static_cast<int>(std::max<std::size_t>(1, static_cast<std::size_t>(width - 2) / std::max<std::size_t>(columnWidth, 1)));
            columns = std::min(columns, 2);
            columns = std::min(columns, static_cast<int>(entries.size()));
            const int rows = static_cast<int>((entries.size() + static_cast<std::size_t>(columns) - 1) / static_cast<std::size_t>(columns));

            for (int row = 0; row < rows; ++row) {
                std::cout << "  ";
                for (int col = 0; col < columns; ++col) {
                    const std::size_t idx = static_cast<std::size_t>(row + col * rows);
                    if (idx >= entries.size()) {
                        continue;
                    }

                    const int currentIndex = menuIndex + static_cast<int>(idx);
                    const std::string indexText = std::to_string(currentIndex) + ")";
                    std::string cell = paint(indexText, kColorIndex) + " " + entries[idx].second;
                    std::cout << cell;

                    if (col + 1 < columns) {
                        const std::size_t cellWidth = visibleWidth(plainCells[idx]);
                        const std::size_t padding = columnWidth > cellWidth ? columnWidth - cellWidth : gutter;
                        std::cout << std::string(padding, ' ');
                    }
                }
                std::cout << '\n';
            }

            for (const auto& [raw, _] : entries) {
                menuFilters.push_back(raw);
            }
            menuIndex += static_cast<int>(entries.size());
        }

        std::string paddedCell(const std::string& text, std::size_t width) {
            const std::size_t currentWidth = visibleWidth(text);
            if (currentWidth >= width) {
                return text;
            }
            return text + std::string(width - currentWidth, ' ');
        }

        std::string centeredCell(const std::string& text, std::size_t width) {
            const std::size_t currentWidth = visibleWidth(text);
            if (currentWidth >= width) {
                return text;
            }
            const std::size_t totalPad = width - currentWidth;
            const std::size_t leftPad = totalPad / 2;
            const std::size_t rightPad = totalPad - leftPad;
            return std::string(leftPad, ' ') + text + std::string(rightPad, ' ');
        }

        std::string subgroupLabel(std::size_t index) {
            std::string label;
            std::size_t value = index;
            do {
                label.insert(label.begin(), static_cast<char>('A' + (value % 26)));
                if (value < 26) {
                    break;
                }
                value = value / 26 - 1;
            } while (true);
            return label;
        }

        std::string renderColoredHeader(const std::string& name, const std::string& label, std::size_t width) {
            const std::string plain = centeredCell(name + " [" + label + "]", width);
            const std::string bracketed = "[" + label + "]";
            const std::size_t labelPos = plain.rfind(bracketed);
            if (labelPos == std::string::npos) {
                return paint(plain, kColorMenuCategory);
            }

            const std::string before = plain.substr(0, labelPos);
            const std::string after = plain.substr(labelPos + bracketed.size());
            return paint(before, kColorMenuCategory) + paint(bracketed, kColorMenuCategoryLabel) + paint(after, kColorMenuCategory);
        }

        void printSubgroupTable(
            const std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>& subgroups,
            int& menuIndex,
            std::vector<std::string>& menuFilters,
            std::unordered_map<std::string, std::vector<std::string>>& subgroupSelections) {
            if (subgroups.empty()) {
                return;
            }

            std::vector<std::string> names;
            names.reserve(subgroups.size());
            for (const auto& [name, _] : subgroups) {
                names.push_back(name);
            }
            std::sort(names.begin(), names.end());

            std::vector<std::vector<std::string>> plainColumns;
            std::vector<std::vector<std::string>> colorColumns;
            std::vector<std::size_t> widths;
            std::vector<std::pair<std::string, std::string>> headers;
            plainColumns.reserve(names.size());
            colorColumns.reserve(names.size());
            widths.reserve(names.size());
            headers.reserve(names.size());

            int nextIndex = menuIndex;
            std::size_t maxRows = 0;

            for (std::size_t subgroupIdx = 0; subgroupIdx < names.size(); ++subgroupIdx) {
                const auto& name = names[subgroupIdx];
                const auto& entries = subgroups.at(name);
                std::vector<std::string> plainCells;
                std::vector<std::string> colorCells;
                const std::string label = subgroupLabel(subgroupIdx);
                const std::string header = name + " [" + label + "]";
                std::size_t width = visibleWidth(header);
                std::vector<std::string> subgroupFilters;

                for (const auto& [raw, pretty] : entries) {
                    const std::string indexText = std::to_string(nextIndex) + ")";
                    const std::string plain = indexText + " " + pretty;
                    const std::string color = paint(indexText, kColorIndex) + " " + pretty;
                    width = std::max(width, visibleWidth(plain));
                    plainCells.push_back(plain);
                    colorCells.push_back(color);
                    menuFilters.push_back(raw);
                    subgroupFilters.push_back(raw);
                    ++nextIndex;
                }

                subgroupSelections[label] = std::move(subgroupFilters);
                maxRows = std::max(maxRows, plainCells.size());
                plainColumns.push_back(std::move(plainCells));
                colorColumns.push_back(std::move(colorCells));
                widths.push_back(width);
                headers.push_back({name, label});
            }

            std::size_t uniformWidth = 0;
            for (const auto width : widths) {
                uniformWidth = std::max(uniformWidth, width);
            }

            auto printBorder = [&](std::size_t columnsInRow) {
                std::cout << paint("  +", kColorMenuGrid);
                for (std::size_t col = 0; col < columnsInRow; ++col) {
                    std::cout << paint(std::string(uniformWidth + 2, '-') + "+", kColorMenuGrid);
                }
                std::cout << '\n';
            };

            constexpr std::size_t kMaxColumnsPerRow = 2;
            for (std::size_t startCol = 0; startCol < names.size(); startCol += kMaxColumnsPerRow) {
                const std::size_t endCol = std::min(startCol + kMaxColumnsPerRow, names.size());
                std::size_t blockRows = 0;
                for (std::size_t col = startCol; col < endCol; ++col) {
                    blockRows = std::max(blockRows, colorColumns[col].size());
                }

                printBorder(endCol - startCol);
                std::cout << paint("  |", kColorMenuGrid);
                for (std::size_t col = startCol; col < endCol; ++col) {
                    std::cout << " " << renderColoredHeader(headers[col].first, headers[col].second, uniformWidth) << " " << paint("|", kColorMenuGrid);
                }
                std::cout << '\n';
                printBorder(endCol - startCol);

                for (std::size_t row = 0; row < blockRows; ++row) {
                    std::cout << paint("  |", kColorMenuGrid);
                    for (std::size_t col = startCol; col < endCol; ++col) {
                        if (row < colorColumns[col].size()) {
                            std::cout << " " << colorColumns[col][row];
                            const std::size_t pad =
                                uniformWidth > visibleWidth(plainColumns[col][row]) ? uniformWidth - visibleWidth(plainColumns[col][row]) : 0;
                            std::cout << std::string(pad, ' ') << " " << paint("|", kColorMenuGrid);
                        } else {
                            std::cout << " " << std::string(uniformWidth, ' ') << " " << paint("|", kColorMenuGrid);
                        }
                    }
                    std::cout << '\n';
                }

                printBorder(endCol - startCol);
            }

            menuIndex = nextIndex;
        }

        Config completeConfigInteractive(Config config,
                                         const fs::path& repoRoot,
                                         const fs::path& benchmarksRoot,
                                         const std::unordered_map<std::string, BenchmarkMeta>& metadata) {
            const fs::path binary = resolveBenchmarkBinary(repoRoot);
            const auto allCases = listBenchmarkCases(binary, "", benchmarksRoot);
            if (allCases.empty()) {
                die("No benchmarks available.");
            }

            std::vector<std::string> filters;
            std::unordered_set<std::string> seen;
            for (const auto& name : allCases) {
                const std::string id = shortBenchId(normalizeBenchmarkCase(name));
                if (seen.insert(id).second) {
                    filters.push_back(id);
                }
            }
            std::sort(filters.begin(), filters.end());

            std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> grouped;
            std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>> subgrouped;
            for (const auto& filter : filters) {
                const auto [mainGroup, subGroup] = classifyByMetadata(filter, metadata);
                const std::string pretty = prettyFilterName(filter, metadata);
                if (!subGroup.empty()) {
                    subgrouped[mainGroup][subGroup].push_back({filter, pretty});
                } else {
                    grouped[mainGroup].push_back({filter, pretty});
                }
            }

            std::cout << kBannerIndent
                      << paint(centeredCell("welcome to LATTICE benchmarks runner", kBannerWidth), kColorTitle) << "\n";

            std::vector<std::string> menuFilters;
            std::unordered_map<std::string, std::vector<std::string>> subgroupSelections;
            std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> allSubgroups;
            std::vector<std::pair<std::string, std::string>> plainEntries;

            for (const auto& [_, plainGroupEntries] : grouped) {
                plainEntries.insert(plainEntries.end(), plainGroupEntries.begin(), plainGroupEntries.end());
            }
            for (const auto& [_, subgroupMap] : subgrouped) {
                for (const auto& [subgroupName, entries] : subgroupMap) {
                    auto& target = allSubgroups[subgroupName];
                    target.insert(target.end(), entries.begin(), entries.end());
                }
            }

            std::sort(plainEntries.begin(), plainEntries.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.second < rhs.second;
            });

            int menuIndex = 1;
            if (!allSubgroups.empty()) {
                std::cout << '\n';
                printSubgroupTable(allSubgroups, menuIndex, menuFilters, subgroupSelections);
            }
            if (!plainEntries.empty()) {
                std::cout << '\n';
                printMenuEntriesColumns(plainEntries, menuIndex, menuFilters);
            }

            std::cout << '\n';
            std::cout << "Choose test (Enter = all): ";
            std::string choice;
            std::getline(std::cin, choice);

            if (!choice.empty()) {
                std::istringstream tokens(choice);
                std::vector<std::string> selected;
                std::string token;
                while (tokens >> token) {
                    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });

                    if (std::all_of(token.begin(), token.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
                        const int idx = std::stoi(token) - 1;
                        if (idx < 0 || idx >= static_cast<int>(menuFilters.size())) {
                            die("Invalid benchmark number.");
                        }
                        selected.push_back(menuFilters[static_cast<std::size_t>(idx)]);
                        continue;
                    }

                    const auto groupIt = subgroupSelections.find(token);
                    if (groupIt != subgroupSelections.end()) {
                        selected.insert(selected.end(), groupIt->second.begin(), groupIt->second.end());
                        continue;
                    }

                    die("Use benchmark numbers or group letters separated by spaces.");
                }

                std::unordered_set<std::string> uniq;
                std::vector<std::string> deduped;
                for (const auto& item : selected) {
                    if (uniq.insert(item).second) {
                        deduped.push_back(item);
                    }
                }

                if (deduped.size() == 1) {
                    config.filter = deduped.front();
                } else if (!deduped.empty()) {
                    std::string regex = "(";
                    for (std::size_t i = 0; i < deduped.size(); ++i) {
                        if (i > 0) {
                            regex += "|";
                        }
                        regex += deduped[i];
                    }
                    regex += ")";
                    config.filter = regex;
                }
            }

            std::vector<std::pair<std::string, std::string>> scenes{
                {"crystal3d", sceneLabel("crystal3d")},
                {"ideal_crystal3d", sceneLabel("ideal_crystal3d")},
                {"crystal2d", sceneLabel("crystal2d")},
                {"random_gas2d", sceneLabel("random_gas2d")},
            };

            std::cout << '\n' << paint("Scene Selection (for SimulationFixture):", kColorTitle) << '\n';
            std::cout << "  " << paint("0)", kColorIndex) << " " << scenes[0].second << " (default)\n";
            for (std::size_t i = 1; i < scenes.size(); ++i) {
                std::cout << "  " << paint(std::to_string(i) + ")", kColorIndex) << " " << scenes[i].second << '\n';
            }
            std::cout << "Scene [0]: ";
            std::string sceneChoice;
            std::getline(std::cin, sceneChoice);
            if (!sceneChoice.empty() && sceneChoice != "0") {
                if (!std::all_of(sceneChoice.begin(), sceneChoice.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
                    die("Invalid scene number.");
                }
                const int idx = std::stoi(sceneChoice);
                if (idx <= 0 || idx >= static_cast<int>(scenes.size())) {
                    die("Invalid scene number.");
                }
                config.scene = scenes[static_cast<std::size_t>(idx)].first;
            }

            std::cout << '\n' << paint("Degradation criterion:", kColorTitle) << '\n';
            std::cout << "  " << paint("0)", kColorIndex) << " Size (default)\n";
            std::cout << "  " << paint("1)", kColorIndex) << " Time\n";
            std::cout << "Criterion [0]: ";
            std::string degradationChoice;
            std::getline(std::cin, degradationChoice);
            if (!degradationChoice.empty() && degradationChoice != "0") {
                if (degradationChoice == "1") {
                    config.degradation = "time";
                } else {
                    die("Invalid degradation criterion.");
                }
            }

            const auto& profile = kRunProfiles[0];
            config.repetitions = profile.repetitions;
            config.minTime = std::string(profile.minTime);
            std::cout << '\n'
                      << paint("Run mode: " + std::string(profile.label) +
                                   " (repetitions=" + std::to_string(config.repetitions) + ", min_time=" + config.minTime + ")",
                               kColorHint)
                      << '\n';

            return config;
        }

        void appendData(BenchmarkData& merged, const BenchmarkData& part) {
            if (merged.rawContextJson.empty()) {
                merged.rawContextJson = part.rawContextJson;
            }
            merged.benchmarks.insert(merged.benchmarks.end(), part.benchmarks.begin(), part.benchmarks.end());
            merged.rawBenchmarkJsons.insert(merged.rawBenchmarkJsons.end(), part.rawBenchmarkJsons.begin(), part.rawBenchmarkJsons.end());
        }

        bool stdinIsInteractive() { return ::isatty(STDIN_FILENO) != 0; }

        std::pair<BenchmarkData, int> mergeCurrentTestsIntoBaseline(const BenchmarkData& baseline, const BenchmarkData& current) {
            std::unordered_set<std::string> currentCases;
            for (const auto& item : current.benchmarks) {
                currentCases.insert(normalizeBenchmarkCase(item.runName));
            }

            BenchmarkData merged;
            merged.rawContextJson = baseline.rawContextJson.empty() ? current.rawContextJson : baseline.rawContextJson;

            int replaced = 0;
            for (std::size_t i = 0; i < baseline.benchmarks.size(); ++i) {
                const auto& item = baseline.benchmarks[i];
                if (currentCases.contains(normalizeBenchmarkCase(item.runName))) {
                    ++replaced;
                    continue;
                }

                merged.benchmarks.push_back(item);
                if (i < baseline.rawBenchmarkJsons.size()) {
                    merged.rawBenchmarkJsons.push_back(baseline.rawBenchmarkJsons[i]);
                }
            }

            merged.benchmarks.insert(merged.benchmarks.end(), current.benchmarks.begin(), current.benchmarks.end());
            merged.rawBenchmarkJsons.insert(merged.rawBenchmarkJsons.end(), current.rawBenchmarkJsons.begin(), current.rawBenchmarkJsons.end());
            return {std::move(merged), replaced};
        }

        void maybeUpdateBaseline(const BenchmarkData& data, const fs::path& resultsPath) {
            if (!stdinIsInteractive()) {
                return;
            }

            std::cout << "\nUpdate baseline with current test data? [y/N]: ";
            std::string answer;
            std::getline(std::cin, answer);
            std::transform(answer.begin(), answer.end(), answer.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (answer != "y") {
                return;
            }

            const fs::path baselinePath = resultsPath / "baseline.json";
            if (const auto oldBaseline = loadBenchmarkDataIfExists(baselinePath)) {
                const auto [merged, replaced] = mergeCurrentTestsIntoBaseline(*oldBaseline, data);
                writeBenchmarkJson(baselinePath, merged);
                std::cout << paint("Baseline updated: " + baselinePath.string() + " (replaced entries: " + std::to_string(replaced) + ")", kColorOk)
                          << '\n';
                return;
            }

            writeBenchmarkJson(baselinePath, data);
            std::cout << paint("Baseline created: " + baselinePath.string(), kColorOk) << '\n';
        }
    }

    BenchmarkData runBenchmarks(const Config& config,
                                const fs::path& repoRoot,
                                const fs::path& benchmarksRoot) {
        const fs::path binary = resolveBenchmarkBinary(repoRoot);
        if (!fs::is_regular_file(binary)) {
            die("Benchmark binary not found: " + binary.string());
        }

        ensureResultsDir(resultsDir(benchmarksRoot));
        const fs::path resultsPath = resultsDir(benchmarksRoot);
        const fs::path outputPath = resultsPath / "_tmp_last_run.json";

        std::cout << '\n' << paint("Selected scene: " + sceneLabel(config.scene), kColorIndexLightBlue) << '\n';
        std::cout << paint("Degradation: " + degradationLabel(config.degradation), kColorIndexLightBlue) << '\n';
        std::cout << paint("Warmup steps: " + std::to_string(config.warmupSteps), kColorIndexLightBlue) << '\n';
        std::cout << paint("Benchmark binary: " + binary.string(), kColorHint) << "\n\n";
        std::cout.flush();

        const auto listedCases = listBenchmarkCases(binary, config.filter, benchmarksRoot);
        std::unordered_set<std::string> uniqueCases;
        for (const auto& name : listedCases) {
            uniqueCases.insert(normalizeBenchmarkCase(name));
        }
        std::vector<std::string> caseList(uniqueCases.begin(), uniqueCases.end());
        std::sort(caseList.begin(), caseList.end());
        if (caseList.empty()) {
            die("No benchmarks match the selected filter.");
        }

        BenchmarkData merged;
        const auto startTs = std::chrono::steady_clock::now();
        int tick = 0;
        redrawProgress(0, static_cast<int>(caseList.size()), "", startTs, tick);

        for (std::size_t i = 0; i < caseList.size(); ++i) {
            const std::string& caseName = caseList[i];
            ++tick;
            redrawProgress(static_cast<int>(i), static_cast<int>(caseList.size()), caseName, startTs, tick);

            const fs::path caseJsonPath = resultsPath / "_tmp_case_run.json";
            std::error_code ec;
            fs::remove(caseJsonPath, ec);

            std::string command;
            command += quoteShell(binary.string());
            command += " --scene=" + quoteShell(config.scene);
            command += " --degradation=" + quoteShell(config.degradation);
            command += " --warmup-steps=" + std::to_string(config.warmupSteps);
            command += " --benchmark_format=console";
            command += " --benchmark_out=" + quoteShell(caseJsonPath.string());
            command += " --benchmark_out_format=json";
            command += " --benchmark_repetitions=" + std::to_string(config.repetitions);
            command += " --benchmark_filter=" + quoteShell("^" + caseName + "$");
            if (!config.minTime.empty()) {
                command += " --benchmark_min_time=" + quoteShell(config.minTime);
            }

            const CommandOutput output = runCommandCapture(command, benchmarksRoot, resultsPath, "case_run");
            if (output.exitCode != 0) {
                std::vector<std::string> tail;
                std::istringstream lines(output.stdoutText + "\n" + output.stderrText);
                std::string line;
                while (std::getline(lines, line)) {
                    if (!line.empty()) {
                        tail.push_back(line);
                    }
                }
                if (!tail.empty()) {
                    std::cerr << '\n';
                    const std::size_t from = tail.size() > 20 ? tail.size() - 20 : 0;
                    for (std::size_t idx = from; idx < tail.size(); ++idx) {
                        std::cerr << tail[idx] << '\n';
                    }
                }
                die("Benchmark '" + caseName + "' failed.");
            }

            if (!fs::is_regular_file(caseJsonPath)) {
                die("Case result JSON was not created: " + caseJsonPath.string());
            }

            appendData(merged, parseBenchmarkJson(readTextFile(caseJsonPath)));
            fs::remove(caseJsonPath, ec);
            redrawProgress(static_cast<int>(i + 1), static_cast<int>(caseList.size()), caseName, startTs, tick);
        }

        if (useColor()) {
            std::cout << "\n\n";
            std::cout.flush();
        }

        writeBenchmarkJson(outputPath, merged);
        const fs::path lastRunPath = resultsPath / "last_run.json";
        std::error_code ec;
        fs::copy_file(outputPath, lastRunPath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            die("Failed to update last_run.json: " + ec.message());
        }

        if (config.save) {
            saveResult(resultsPath, outputPath, config.filter);
        }

        fs::remove(outputPath, ec);
        return merged;
    }
}

int main(int argc, char** argv) {
    auto config = Benchmarks::BmRunner::parseArgs(argc, argv);
    const fs::path benchmarksRoot = Benchmarks::BmRunner::benchmarksRootFromExecutable(argv[0]);
    const fs::path repoRoot = benchmarksRoot.parent_path().parent_path();

    Benchmarks::BmRunner::printBanner();

    if (config.list) {
        const auto files = Benchmarks::BmRunner::savedResults(Benchmarks::BmRunner::resultsDir(benchmarksRoot));
        if (files.empty()) {
            std::cout << "No saved results.\n";
            return 0;
        }

        std::cout << '\n' << Benchmarks::BmRunner::paint("Saved results:", Benchmarks::BmRunner::kColorTitle) << "\n\n";
        for (const auto& file : files) {
            std::cout << "  " << Benchmarks::BmRunner::formatTimestamp(file) << "  —  " << file.string() << '\n';
        }
        return 0;
    }

    const auto metadata = Benchmarks::BmRunner::loadBenchMetadata(benchmarksRoot);
    if (config.filter.empty()) {
        config = Benchmarks::BmRunner::completeConfigInteractive(std::move(config), repoRoot, benchmarksRoot, metadata);
    }

    const auto data = Benchmarks::BmRunner::runBenchmarks(config, repoRoot, benchmarksRoot);
    Benchmarks::BmRunner::printResultsTable(
        data, metadata, Benchmarks::BmRunner::resultsDir(benchmarksRoot), config.scene, config.degradation);
    if (config.degradation == "size") {
        Benchmarks::BmRunner::maybeUpdateBaseline(data, Benchmarks::BmRunner::resultsDir(benchmarksRoot));
    }
    return 0;
}
